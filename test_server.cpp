#include "dolphin_runtime.hpp"
using namespace DolphinRuntime;

var server;
var tcpServer;

int main() {
    std::srand(std::time(nullptr));
    print(var("--- Starting Dolphin Networking Engines ---"));
    server = HTTP.Server();
    server.get(var("/"), [=](var req, var res) mutable {
            print(var("  [HTTP] GET / received"));
            res.send(var("Welcome to Dolphin HTTP Server!"));
        }
    );
    server.get(var("/users/:id"), [=](var req, var res) mutable {
            var user_id = req[var("params")][var("id")];
            print(((var("  [HTTP] GET /users/") + user_id) + var(" received")));
            res.json(var_object{{"userId", user_id}, {"active", true}, {"engine", var("Dolphin")}});
        }
    );
    server.post(var("/echo"), [=](var req, var res) mutable {
            print(var("  [HTTP] POST /echo received"));
            res.send(var("Echo Post Request Success"));
        }
    );
    server.listen(var(8080));
    print(var("HTTP Server listening on http://localhost:8080"));
    tcpServer = TCP.Server();
    tcpServer.on(var("connection"), [=](var client) mutable {
            print(var("  [TCP] New client connected to port 9090!"));
            client.on(var("data"), [=](var data) mutable {
                var trimmed = data.trim();
                print((var("  [TCP Data Received] ") + trimmed));
                client.write(((var("ACK: ") + trimmed) + var("\n")));
                if ((trimmed == var("quit"))) {
                    print(var("  [TCP] Close command received from client."));
                    client.close();
                }
            }
    );
            client.on(var("close"), [=]() mutable {
                print(var("  [TCP] Client connection closed."));
            }
    );
        }
    );
    tcpServer.listen(var(9090));
    print(var("TCP Server listening on port 9090"));
    print(var("All servers running. Waiting for connections..."));
    DolphinRuntime::runEventLoop();
    return 0;
}
