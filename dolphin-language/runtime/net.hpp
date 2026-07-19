#pragma once
#include "async.hpp"
#include <fstream>

// ── MIME type helper ─────────────────────────────────────────────────────────
inline std::string dolphin_mime_type(const std::string& path) {
    auto ext_pos = path.rfind('.');
    std::string ext = (ext_pos != std::string::npos) ? path.substr(ext_pos) : "";
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css")  return "text/css; charset=utf-8";
    if (ext == ".js")   return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")  return "image/gif";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".ico")  return "image/x-icon";
    if (ext == ".woff" || ext == ".woff2") return "font/woff2";
    if (ext == ".ttf")  return "font/ttf";
    if (ext == ".mp3")  return "audio/mpeg";
    if (ext == ".mp4")  return "video/mp4";
    if (ext == ".pdf")  return "application/pdf";
    if (ext == ".txt")  return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

// Read file into string (binary-safe)
inline bool dolphin_read_file(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>(f), {});
    return true;
}

#ifdef _WIN32
struct WinsockInit {
    WinsockInit() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    ~WinsockInit() {
        WSACleanup();
    }
};
inline WinsockInit g_winsock_init;
#endif

// TCP Socket wrapper
class TCPSocketClass {
public:
    SOCKET sock;
    std::shared_ptr<std::map<std::string, std::vector<var>>> event_listeners;
    std::vector<std::pair<std::string, std::string>> extra_headers;

    TCPSocketClass(SOCKET s, std::shared_ptr<std::map<std::string, std::vector<var>>> listeners) 
        : sock(s), event_listeners(listeners) {
        DolphinRuntime::EventLoop::instance().ref();
    }
    ~TCPSocketClass() {
        close();
        DolphinRuntime::EventLoop::instance().unref();
    }

    void write(const std::string& data) {
        if (sock != INVALID_SOCKET) {
            ::send(sock, data.c_str(), (int)data.length(), 0);
        }
    }

    void close() {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
            trigger("close", var());
        }
    }

    void trigger(const std::string& event, const var& data) {
        if (event_listeners && event_listeners->count(event)) {
            std::vector<var> current_listeners = (*event_listeners)[event];
            for (const auto& cb : current_listeners) {
                DolphinRuntime::EventLoop::instance().queueCallback(cb, {data});
            }
        }
    }

    void startReadLoop() {
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            char buffer[4096];
            while (sock != INVALID_SOCKET) {
                int bytes = ::recv(sock, buffer, sizeof(buffer) - 1, 0);
                if (bytes <= 0) {
                    #ifdef _WIN32
                        std::cout << "recv error: " << WSAGetLastError() << std::endl;
                    #else
                        std::cout << "recv error: " << errno << std::endl;
                    #endif
                    break;
                }
                buffer[bytes] = '\0';
                trigger("data", var(std::string(buffer)));
            }
            close();
        }).detach();
    }
};

// TCP Server class
class TCPServerClass {
public:
    SOCKET server_sock = INVALID_SOCKET;
    std::shared_ptr<std::map<std::string, std::vector<var>>> event_listeners;
    std::vector<var> active_connections;
    std::mutex connections_mutex;

    TCPServerClass(std::shared_ptr<std::map<std::string, std::vector<var>>> listeners) 
        : event_listeners(listeners) {}
    ~TCPServerClass() {
        if (server_sock != INVALID_SOCKET) {
            closesocket(server_sock);
        }
    }

    void trigger(const std::string& event, const var& data) {
        if (event_listeners && event_listeners->count(event)) {
            std::vector<var> current_listeners = (*event_listeners)[event];
            for (const auto& cb : current_listeners) {
                DolphinRuntime::EventLoop::instance().queueCallback(cb, {data});
            }
        }
    }

    void listen(int port) {
        server_sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock == INVALID_SOCKET) return;

        int opt = 1;
        #ifdef _WIN32
            setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
        #else
            setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        #endif

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (::bind(server_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
#ifdef _WIN32
            print("[TCP Server] Bind failed with error: " + std::to_string(WSAGetLastError()));
#else
            print("[TCP Server] Bind failed");
#endif
            closesocket(server_sock);
            return;
        }

        if (::listen(server_sock, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(server_sock);
            return;
        }

        DolphinRuntime::EventLoop::instance().ref();
        std::thread([this]() {
            while (server_sock != INVALID_SOCKET) {
                SOCKET client = ::accept(server_sock, nullptr, nullptr);
                if (client == INVALID_SOCKET) break;

                // Auto Thread Cloning accepting loop
                std::thread([this, client]() {
                    var socket_client = var(var_object{});
                    socket_client.event_listeners = std::make_shared<std::map<std::string, std::vector<var>>>();
                    socket_client.tcp_socket = std::make_shared<TCPSocketClass>(client, socket_client.event_listeners);
                    
                    socket_client["write"] = var([socket_client](const var& data) mutable -> var {
                        if (socket_client.tcp_socket) socket_client.tcp_socket->write(data.toString());
                        return socket_client;
                    });
                    
                    socket_client["close"] = var([socket_client]() mutable -> var {
                        if (socket_client.tcp_socket) socket_client.tcp_socket->close();
                        return socket_client;
                    });
                    
                    socket_client.tcp_socket->startReadLoop();

                    {
                        std::lock_guard<std::mutex> lock(connections_mutex);
                        active_connections.push_back(socket_client);
                    }

                    socket_client.on("close", var([this, client](const std::vector<var>& args) -> var {
                        std::lock_guard<std::mutex> lock(connections_mutex);
                        active_connections.erase(
                            std::remove_if(active_connections.begin(), active_connections.end(),
                                [client](const var& c) {
                                    return c.tcp_socket && c.tcp_socket->sock == client;
                                }),
                            active_connections.end()
                        );
                        return var();
                    }));

                    trigger("connection", socket_client);
                }).detach();
            }
        }).detach();
    }
};

// HTTP Request and Response wrappers
struct HTTPResponse {
    SOCKET client;
    int status_code = 200;
    std::map<std::string, std::string> response_headers;
    bool sent = false;

    HTTPResponse(SOCKET s) : client(s) {
        response_headers["Content-Type"] = "text/html";
    }

    void status(int code) {
        status_code = code;
    }

    void header(const std::string& key, const std::string& val) {
        response_headers[key] = val;
    }

    void send(const std::string& body) {
        if (sent) return;
        sent = true;

        std::string status_text = "OK";
        if (status_code == 404) status_text = "Not Found";
        else if (status_code == 500) status_text = "Internal Server Error";

        std::stringstream ss;
        ss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
        for (auto const& [key, val] : response_headers) {
            ss << key << ": " << val << "\r\n";
        }
        ss << "Content-Length: " << body.length() << "\r\n";
        ss << "Connection: close\r\n\r\n";
        ss << body;

        ::send(client, ss.str().c_str(), (int)ss.str().length(), 0);
        closesocket(client);
    }
};

// HTTP Server class
class HTTPServerClass {
public:
    SOCKET server_sock = INVALID_SOCKET;
    std::vector<std::pair<std::string, var>> get_routes;
    std::vector<std::pair<std::string, var>> post_routes;
    std::string static_dir;   // directory served by server.static()

    ~HTTPServerClass() {
        if (server_sock != INVALID_SOCKET) {
            closesocket(server_sock);
        }
    }

    void get(const std::string& route_path, const var& callback) {
        get_routes.push_back({route_path, callback});
    }

    void post(const std::string& route_path, const var& callback) {
        post_routes.push_back({route_path, callback});
    }

    // Serve every file inside dir as a static route
    void serve_static(const std::string& dir) {
        static_dir = dir;
        // Strip trailing slash
        while (!static_dir.empty() && (static_dir.back() == '/' || static_dir.back() == '\\'))
            static_dir.pop_back();
    }

    bool matchRoute(const std::string& route_tmpl, const std::string& req_path, var& params) {
        std::vector<std::string> tmpl_parts;
        std::vector<std::string> path_parts;

        std::stringstream ss_tmpl(route_tmpl);
        std::string part;
        while (std::getline(ss_tmpl, part, '/')) {
            if (!part.empty()) tmpl_parts.push_back(part);
        }

        std::stringstream ss_path(req_path);
        while (std::getline(ss_path, part, '/')) {
            if (!part.empty()) path_parts.push_back(part);
        }

        if (tmpl_parts.size() != path_parts.size()) return false;

        var temp_params = var_object{};
        for (size_t i = 0; i < tmpl_parts.size(); ++i) {
            if (tmpl_parts[i][0] == ':') {
                std::string param_name = tmpl_parts[i].substr(1);
                temp_params[param_name] = var(path_parts[i]);
            } else if (tmpl_parts[i] != path_parts[i]) {
                return false;
            }
        }
        params = temp_params;
        return true;
    }

    void listen(int port) {
        server_sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock == INVALID_SOCKET) return;

        int opt = 1;
        #ifdef _WIN32
            setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
        #else
            setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        #endif

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (::bind(server_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
#ifdef _WIN32
            print("[HTTP Server] Bind failed with error: " + std::to_string(WSAGetLastError()));
#else
            print("[HTTP Server] Bind failed");
#endif
            closesocket(server_sock);
            return;
        }

        if (::listen(server_sock, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(server_sock);
            return;
        }

        DolphinRuntime::EventLoop::instance().ref();
        std::thread([this]() {
            while (server_sock != INVALID_SOCKET) {
                SOCKET client = ::accept(server_sock, nullptr, nullptr);
                if (client == INVALID_SOCKET) break;

                // Auto Thread Cloning
                std::thread([this, client]() {
                    char buffer[8192];
                    int bytes = ::recv(client, buffer, sizeof(buffer) - 1, 0);
                    if (bytes <= 0) {
                        closesocket(client);
                        return;
                    }
                    buffer[bytes] = '\0';

                    std::string raw_req(buffer, bytes);
                    size_t body_pos = raw_req.find("\r\n\r\n");
                    
                    // Parse Content-Length if present
                    size_t content_length = 0;
                    size_t cl_pos = raw_req.find("Content-Length:");
                    if (cl_pos == std::string::npos) {
                        cl_pos = raw_req.find("content-length:");
                    }
                    if (cl_pos != std::string::npos) {
                        size_t val_start = cl_pos + 15;
                        size_t val_end = raw_req.find("\r\n", val_start);
                        if (val_end != std::string::npos) {
                            std::string cl_str = raw_req.substr(val_start, val_end - val_start);
                            while (!cl_str.empty() && std::isspace((unsigned char)cl_str.front())) cl_str.erase(cl_str.begin());
                            while (!cl_str.empty() && std::isspace((unsigned char)cl_str.back())) cl_str.pop_back();
                            try {
                                content_length = std::stoul(cl_str);
                            } catch (...) {}
                        }
                    }

                    if (content_length > 0 && body_pos != std::string::npos) {
                        size_t body_read = raw_req.length() - (body_pos + 4);
                        while (body_read < content_length) {
                            int r = ::recv(client, buffer, std::min<size_t>(sizeof(buffer) - 1, content_length - body_read), 0);
                            if (r <= 0) break;
                            raw_req.append(buffer, r);
                            body_read += r;
                        }
                    }
                    std::stringstream ss(raw_req);
                    std::string line;

                    std::getline(ss, line);
                    std::stringstream req_line_ss(line);
                    std::string method, full_path, version;
                    req_line_ss >> method >> full_path >> version;

                    std::string path = full_path;
                    var query = var_object{};
                    size_t q_pos = full_path.find('?');
                    if (q_pos != std::string::npos) {
                        path = full_path.substr(0, q_pos);
                        std::string query_str = full_path.substr(q_pos + 1);
                        std::stringstream q_ss(query_str);
                        std::string pair;
                        while (std::getline(q_ss, pair, '&')) {
                            size_t eq_pos = pair.find('=');
                            if (eq_pos != std::string::npos) {
                                query[pair.substr(0, eq_pos)] = var(pair.substr(eq_pos + 1));
                            } else {
                                query[pair] = var("");
                            }
                        }
                    }

                    var route_callback;
                    var params = var_object{};
                    auto& routes = (method == "POST") ? post_routes : get_routes;

                    // ── CORS preflight ───────────────────────────────────
                    if (method == "OPTIONS") {
                        HTTPResponse res(client);
                        res.status(204);
                        res.header("Access-Control-Allow-Origin", "*");
                        res.header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
                        res.header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
                        res.send("");
                        goto next_request;
                    }

                    for (auto const& [route_tmpl, cb] : routes) {
                        if (matchRoute(route_tmpl, path, params)) {
                            route_callback = cb;
                            break;
                        }
                    }

                    // ── Static file serving ──────────────────────────────
                    if (!route_callback.isFunction() && !static_dir.empty() && method == "GET") {
                        // Try exact path, then path + /index.html
                        std::vector<std::string> try_paths = {
                            static_dir + path,
                            static_dir + path + "/index.html",
                            static_dir + "/index.html"
                        };
                        for (auto& fpath : try_paths) {
                            std::string file_content;
                            if (dolphin_read_file(fpath, file_content)) {
                                HTTPResponse res(client);
                                res.header("Content-Type", dolphin_mime_type(fpath));
                                res.header("Access-Control-Allow-Origin", "*");
                                res.send(file_content);
                                goto next_request;
                            }
                        }
                    }

                    if (route_callback.isFunction()) {
                        // Parse POST/PUT body if present
                        if (method == "POST" || method == "PUT") {
                            size_t body_pos = raw_req.find("\r\n\r\n");
                            if (body_pos != std::string::npos) {
                                std::string body = raw_req.substr(body_pos + 4);
                                // Trim leading/trailing whitespace
                                while (!body.empty() && std::isspace((unsigned char)body.front())) body.erase(body.begin());
                                while (!body.empty() && std::isspace((unsigned char)body.back())) body.pop_back();

                                if (!body.empty()) {
                                    // 1. Try JSON parsing (flat object)
                                    if (body.front() == '{' && body.back() == '}') {
                                        size_t idx = 1;
                                        while (idx < body.size() - 1) {
                                            size_t key_start = body.find('"', idx);
                                            if (key_start == std::string::npos || key_start >= body.size() - 1) break;
                                            size_t key_end = body.find('"', key_start + 1);
                                            if (key_end == std::string::npos) break;
                                            std::string key = body.substr(key_start + 1, key_end - key_start - 1);

                                            size_t colon = body.find(':', key_end + 1);
                                            if (colon == std::string::npos) break;

                                            size_t val_start = colon + 1;
                                            while (val_start < body.size() - 1 && std::isspace((unsigned char)body[val_start])) val_start++;
                                            
                                            std::string val_str;
                                            if (body[val_start] == '"') {
                                                size_t val_end = body.find('"', val_start + 1);
                                                if (val_end == std::string::npos) break;
                                                val_str = body.substr(val_start + 1, val_end - val_start - 1);
                                                params[key] = var(val_str);
                                                idx = val_end + 1;
                                            } else {
                                                size_t val_end = val_start;
                                                while (val_end < body.size() - 1 && body[val_end] != ',' && body[val_end] != '}') {
                                                    val_end++;
                                                }
                                                std::string val = body.substr(val_start, val_end - val_start);
                                                while (!val.empty() && std::isspace((unsigned char)val.back())) val.pop_back();
                                                if (val == "true") params[key] = var(true);
                                                else if (val == "false") params[key] = var(false);
                                                else if (val == "null") params[key] = var();
                                                else {
                                                    try {
                                                        if (val.find('.') != std::string::npos) {
                                                            params[key] = var(std::stod(val));
                                                        } else {
                                                            params[key] = var(std::stoll(val));
                                                        }
                                                    } catch (...) {
                                                        params[key] = var(val);
                                                    }
                                                }
                                                idx = val_end + 1;
                                            }
                                        }
                                    }
                                    // 2. Try URL-encoded parsing (key1=val1&key2=val2)
                                    else if (body.find('=') != std::string::npos) {
                                        std::stringstream b_ss(body);
                                        std::string pair;
                                        while (std::getline(b_ss, pair, '&')) {
                                            size_t eq_pos = pair.find('=');
                                            if (eq_pos != std::string::npos) {
                                                std::string k = pair.substr(0, eq_pos);
                                                std::string v = pair.substr(eq_pos + 1);
                                                std::string decoded;
                                                for (size_t i = 0; i < v.length(); ++i) {
                                                    if (v[i] == '+') decoded += ' ';
                                                    else if (v[i] == '%' && i + 2 < v.length()) {
                                                        char hex[3] = { v[i+1], v[i+2], '\0' };
                                                        decoded += (char)std::strtol(hex, nullptr, 16);
                                                        i += 2;
                                                    } else {
                                                        decoded += v[i];
                                                    }
                                                }
                                                params[k] = var(decoded);
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        var req_var = var_object{
                            {"method", var(method)},
                            {"path", var(path)},
                            {"params", params},
                            {"query", query}
                        };

                        var res_var = var(var_object{});
                        res_var.tcp_socket = std::make_shared<TCPSocketClass>(client, nullptr);
                        
                        res_var["send"] = var([res_var](const var& body) mutable -> var {
                            if (res_var.tcp_socket) {
                                HTTPResponse res(res_var.tcp_socket->sock);
                                res.status(res_var.http_status);
                                for (auto& h : res_var.tcp_socket->extra_headers)
                                    res.header(h.first, h.second);
                                res.send(body.toString());
                            }
                            return res_var;
                        });

                        // res.file(path) — serve a file from disk
                        res_var["file"] = var([res_var](const var& fpath) mutable -> var {
                            if (res_var.tcp_socket) {
                                std::string file_content;
                                HTTPResponse res(res_var.tcp_socket->sock);
                                res.status(res_var.http_status);
                                for (auto& h : res_var.tcp_socket->extra_headers)
                                    res.header(h.first, h.second);
                                if (dolphin_read_file(fpath.toString(), file_content)) {
                                    res.header("Content-Type", dolphin_mime_type(fpath.toString()));
                                    res.send(file_content);
                                } else {
                                    res.status(404);
                                    res.header("Content-Type", "text/plain");
                                    res.send("File not found: " + fpath.toString());
                                }
                            }
                            return res_var;
                        });

                        // res.setHeader(key, val) — set a custom header
                        res_var["setHeader"] = var([res_var](const var& key, const var& val) mutable -> var {
                            if (res_var.tcp_socket) {
                                res_var.tcp_socket->extra_headers.emplace_back(
                                    key.toString(), val.toString());
                            }
                            return res_var;
                        });

                        // res.cors() — add CORS headers for API routes
                        res_var["cors"] = var([res_var](const std::vector<var>& _) mutable -> var {
                            if (res_var.tcp_socket) {
                                res_var.tcp_socket->extra_headers.emplace_back(
                                    "Access-Control-Allow-Origin", "*");
                                res_var.tcp_socket->extra_headers.emplace_back(
                                    "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
                            }
                            return res_var;
                        });
                        
                        res_var["json"] = var([res_var](const var& body) mutable -> var {
                            if (res_var.tcp_socket) {
                                HTTPResponse res(res_var.tcp_socket->sock);
                                res.status(res_var.http_status);
                                for (auto& h : res_var.tcp_socket->extra_headers)
                                    res.header(h.first, h.second);
                                res.header("Content-Type", "application/json");
                                res.send(body.toString());
                            }
                            return res_var;
                        });
                        
                        res_var["status"] = var([res_var](const var& code) mutable -> var {
                            const_cast<var&>(res_var).http_status = (int)code.toInt();
                            return res_var;
                        });

                        // res.html(body) — send HTML response
                        res_var["html"] = var([res_var](const var& body) mutable -> var {
                            if (res_var.tcp_socket) {
                                HTTPResponse res(res_var.tcp_socket->sock);
                                res.status(res_var.http_status);
                                for (auto& h : res_var.tcp_socket->extra_headers)
                                    res.header(h.first, h.second);
                                res.header("Content-Type", "text/html; charset=utf-8");
                                res.send(body.toString());
                            }
                            return res_var;
                        });

                        // res.redirect(url) — send 302 redirect
                        res_var["redirect"] = var([res_var](const var& url) mutable -> var {
                            if (res_var.tcp_socket) {
                                HTTPResponse res(res_var.tcp_socket->sock);
                                res.status(302);
                                res.header("Location", url.toString());
                                res.send("");
                            }
                            return res_var;
                        });

                        DolphinRuntime::EventLoop::instance().queueCallback(route_callback, {req_var, res_var});
                    } else {
                        HTTPResponse res(client);
                        res.status(404);
                        res.header("Content-Type", "text/html; charset=utf-8");
                        res.send("<html><body style='font-family:sans-serif;padding:40px'>"
                                 "<h2>404 &#8212; Not Found</h2>"
                                 "<p>Path <code>" + path + "</code> does not exist.</p>"
                                 "<a href='/'>&#8592; Home</a></body></html>");
                    }
                    next_request:;
                }).detach();
            }
        }).detach();
    }
};

inline void PromiseClass::resolve(const var& val) {
    std::unique_lock<std::mutex> lock(mtx);
    value_ptr = std::make_shared<var>(val);
    resolved = true;
    cv.notify_all();
}

inline var PromiseClass::await_resolve() {
    std::unique_lock<std::mutex> lock(mtx);
    while (!resolved) {
        cv.wait(lock);
    }
    if (value_ptr) return *value_ptr;
    return var();
}

inline var var::Promise() {
    return var(std::make_shared<PromiseClass>());
}

inline void var::resolve(const var& val) {
    if (type == TYPE_PROMISE && promise_val) {
        promise_val->resolve(val);
    }
}

inline var var::await_value() const {
    if (type == TYPE_PROMISE && promise_val) {
        return promise_val->await_resolve();
    }
    return *this;
}

inline var var::emit(const std::string& event, const var& data) {
    if (event_listeners && event_listeners->count(event)) {
        std::vector<var> current_listeners = (*event_listeners)[event];
        for (const auto& cb : current_listeners) {
            if (cb.isFunction()) {
                if (DolphinRuntime::EventLoop::instance().isMainThread()) {
                    cb(std::vector<var>{data});
                } else {
                    DolphinRuntime::EventLoop::instance().queueCallback(cb, {data});
                }
            }
        }
    }
    return *this;
}

inline var var::MatrixCreate(int r, int c, const var& data) {
    std::vector<double> flat_data;
    if (data.isArray() && data.array_val) {
        flat_data.reserve(r * c);
        for (const auto& row : *data.array_val) {
            if (row.isArray() && row.array_val) {
                for (const auto& elem : *row.array_val) {
                    flat_data.push_back(elem.toDouble());
                }
            } else {
                flat_data.push_back(row.toDouble());
            }
        }
    }
    while ((int)flat_data.size() < r * c) flat_data.push_back(0.0);
    if ((int)flat_data.size() > r * c) flat_data.resize(r * c);
    return var(std::make_shared<MatrixClass>(r, c, flat_data));
}

inline var var::MatrixZeros(int r, int c) {
    return var(std::make_shared<MatrixClass>(r, c));
}

inline var var::MatrixOnes(int r, int c) {
    return var(std::make_shared<MatrixClass>(r, c, std::vector<double>(r * c, 1.0)));
}

inline var var::MatrixRandom(int r, int c) {
    std::vector<double> d(r * c);
    for (int i = 0; i < r * c; ++i) {
        d[i] = ((double)rand() / RAND_MAX) * 2.0 - 1.0;
    }
    return var(std::make_shared<MatrixClass>(r, c, d));
}

inline var var::matmul(const var& other) const {
    if (type != TYPE_MATRIX || !matrix_val || other.type != TYPE_MATRIX || !other.matrix_val) {
        return var();
    }
    int r1 = matrix_val->rows;
    int c1 = matrix_val->cols;
    int r2 = other.matrix_val->rows;
    int c2 = other.matrix_val->cols;
    if (c1 != r2) {
        print("[Matrix Error] matmul dimensions mismatch: " + std::to_string(r1) + "x" + std::to_string(c1) + " and " + std::to_string(r2) + "x" + std::to_string(c2));
        return var();
    }
    std::shared_ptr<MatrixClass> res = std::make_shared<MatrixClass>(r1, c2);
    for (int i = 0; i < r1; ++i) {
        for (int j = 0; j < c2; ++j) {
            double sum = 0.0;
            for (int k = 0; k < c1; ++k) {
                sum += matrix_val->get(i, k) * other.matrix_val->get(k, j);
            }
            res->set(i, j, sum);
        }
    }
    return var(res);
}

inline var var::transpose() const {
    if (type != TYPE_MATRIX || !matrix_val) return var();
    int r = matrix_val->rows;
    int c = matrix_val->cols;
    std::shared_ptr<MatrixClass> res = std::make_shared<MatrixClass>(c, r);
    for (int i = 0; i < r; ++i) {
        for (int j = 0; j < c; ++j) {
            res->set(j, i, matrix_val->get(i, j));
        }
    }
    return var(res);
}

inline var var::sigmoid() const {
    if (type != TYPE_MATRIX || !matrix_val) return var();
    std::shared_ptr<MatrixClass> res = std::make_shared<MatrixClass>(matrix_val->rows, matrix_val->cols);
    for (size_t i = 0; i < matrix_val->data.size(); ++i) {
        res->data[i] = 1.0 / (1.0 + exp(-matrix_val->data[i]));
    }
    return var(res);
}

inline var var::sigmoidDerivative() const {
    if (type != TYPE_MATRIX || !matrix_val) return var();
    std::shared_ptr<MatrixClass> res = std::make_shared<MatrixClass>(matrix_val->rows, matrix_val->cols);
    for (size_t i = 0; i < matrix_val->data.size(); ++i) {
        double s = matrix_val->data[i];
        res->data[i] = s * (1.0 - s);
    }
    return var(res);
}

inline var var::relu() const {
    if (type != TYPE_MATRIX || !matrix_val) return var();
    std::shared_ptr<MatrixClass> res = std::make_shared<MatrixClass>(matrix_val->rows, matrix_val->cols);
    for (size_t i = 0; i < matrix_val->data.size(); ++i) {
        res->data[i] = std::max(0.0, matrix_val->data[i]);
    }
    return var(res);
}

inline var var::reluDerivative() const {
    if (type != TYPE_MATRIX || !matrix_val) return var();
    std::shared_ptr<MatrixClass> res = std::make_shared<MatrixClass>(matrix_val->rows, matrix_val->cols);
    for (size_t i = 0; i < matrix_val->data.size(); ++i) {
        res->data[i] = matrix_val->data[i] > 0.0 ? 1.0 : 0.0;
    }
    return var(res);
}

inline var var::toArray() const {
    if (type != TYPE_MATRIX || !matrix_val) return var();
    var_array res;
    int r = matrix_val->rows;
    int c = matrix_val->cols;
    for (int i = 0; i < r; ++i) {
        var_array row;
        for (int j = 0; j < c; ++j) {
            row.push_back(var(matrix_val->get(i, j)));
        }
        res.push_back(var(row));
    }
    return var(res);
}

// Inline var class method definitions
inline var var::get(const var& path, const var& cb) {
    if (http_server) http_server->get(path.toString(), cb);
    return *this;
}

inline var var::post(const var& path, const var& cb) {
    if (http_server) http_server->post(path.toString(), cb);
    return *this;
}

inline var var::serve_static(const var& dir) {
    if (http_server) http_server->serve_static(dir.toString());
    return *this;
}

inline var var::listen(const var& port) {
    if (http_server) http_server->listen((int)port.toInt());
    if (tcp_server) tcp_server->listen((int)port.toInt());
    return *this;
}

inline var var::write(const var& data) {
    if (tcp_socket) tcp_socket->write(data.toString());
    return *this;
}

inline var var::close() {
    if (tcp_socket) tcp_socket->close();
    return *this;
}

inline var var::status(const var& code) {
    http_status = (int)code.toInt();
    return *this;
}

inline var var::send(const var& body) {
    if (tcp_socket) {
        HTTPResponse res(tcp_socket->sock);
        res.status(http_status);
        res.send(body.toString());
    }
    return *this;
}

inline var var::json(const var& body) {
    if (tcp_socket) {
        HTTPResponse res(tcp_socket->sock);
        res.status(http_status);
        res.header("Content-Type", "application/json");
        res.send(body.toString());
    }
    return *this;
}
