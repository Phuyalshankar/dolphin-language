#pragma once
#include "async.hpp"

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

                    std::string raw_req(buffer);
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

                    for (auto const& [route_tmpl, cb] : routes) {
                        if (matchRoute(route_tmpl, path, params)) {
                            route_callback = cb;
                            break;
                        }
                    }

                    if (route_callback.isFunction()) {
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
                                res.send(body.toString());
                            }
                            return res_var;
                        });
                        
                        res_var["json"] = var([res_var](const var& body) mutable -> var {
                            if (res_var.tcp_socket) {
                                HTTPResponse res(res_var.tcp_socket->sock);
                                res.status(res_var.http_status);
                                res.header("Content-Type", "application/json");
                                res.send(body.toString());
                            }
                            return res_var;
                        });
                        
                        res_var["status"] = var([res_var](const var& code) mutable -> var {
                            const_cast<var&>(res_var).http_status = (int)code.toInt();
                            return res_var;
                        });

                        DolphinRuntime::EventLoop::instance().queueCallback(route_callback, {req_var, res_var});
                    } else {
                        HTTPResponse res(client);
                        res.status(404);
                        res.send("Not Found");
                    }
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
