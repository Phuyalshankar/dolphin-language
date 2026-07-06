#pragma once
// Real-hardware runtime for Dolphin's `dolphin flash` target.
//
// This header is compiled by the Arduino/ESP32/ESP8266/STM32 toolchains
// (via arduino-cli), NOT by the PC `g++` path used by `dolphin run`.
// It mirrors the public API of dolphin_runtime.hpp's `var`, `Math`, and
// `JSON` helpers, but swaps the `Pin` class for real GPIO calls and routes
// print()/input() through the board's Serial port instead of stdio.
//
// Supported boards (see dolphin.cpp `flash` command for the FQBN table):
//   - ESP32, ESP8266: full support, these toolchains ship a full libstdc++.
//   - STM32 (STM32duino core): full support.
//   - Arduino AVR (Uno/Nano/Mega): best-effort. Stock avr-gcc has no
//     libstdc++, so std::string/std::vector/std::function used by `var`
//     will only compile if the sketch also depends on a Serial-STL shim
//     (e.g. ArduinoSTL) installed in arduino-cli. Prefer ESP32/STM32 for
//     anything beyond simple digital I/O on AVR boards.

#include <Arduino.h>
#undef PI

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
using WebServerClass = ESP8266WebServer;
#else
#include <WiFi.h>
#include <WebServer.h>
using WebServerClass = WebServer;
#endif

static WebServerClass* global_esp_web_server = nullptr;

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <memory>
#include <initializer_list>
#include <functional>
#include <map>
#include <cmath>
#include <cstdlib>

class var;
using var_array = std::vector<var>;
using var_object = std::map<std::string, var>;

class var {
public:
    enum Type { TYPE_NULL, TYPE_BOOL, TYPE_INT, TYPE_DOUBLE, TYPE_STRING, TYPE_ARRAY, TYPE_OBJECT, TYPE_FUNCTION };

private:
    Type type = TYPE_NULL;
    bool bool_val = false;
    long long int_val = 0;
    double double_val = 0.0;
    std::string string_val = "";
    std::shared_ptr<var_array> array_val = nullptr;
    std::shared_ptr<var_object> object_val = nullptr;
    std::function<var(const std::vector<var>&)> func_val = nullptr;

public:
    var() : type(TYPE_NULL) {}
    var(bool v) : type(TYPE_BOOL), bool_val(v) {}
    var(int v) : type(TYPE_INT), int_val(v) {}
    var(long v) : type(TYPE_INT), int_val(v) {}
    var(long long v) : type(TYPE_INT), int_val(v) {}
    var(double v) : type(TYPE_DOUBLE), double_val(v) {}
    var(const char* v) : type(TYPE_STRING), string_val(v) {}
    var(const std::string& v) : type(TYPE_STRING), string_val(v) {}
    var(const var_array& v) : type(TYPE_ARRAY), array_val(std::make_shared<var_array>(v)) {}
    var(std::initializer_list<var> list) : type(TYPE_ARRAY), array_val(std::make_shared<var_array>(list)) {}
    var(const var_object& o) : type(TYPE_OBJECT), object_val(std::make_shared<var_object>(o)) {}

    template<typename F, typename = std::enable_if_t<
        !std::is_same_v<std::decay_t<F>, var> &&
        (std::is_invocable_v<F> ||
         std::is_invocable_v<F, var> ||
         std::is_invocable_v<F, var, var> ||
         std::is_invocable_v<F, const std::vector<var>&>)
    >>
    var(F&& f) : type(TYPE_FUNCTION) {
        func_val = [f = std::forward<F>(f)](const std::vector<var>& args) mutable -> var {
            if constexpr (std::is_invocable_r_v<var, F, const std::vector<var>&>) {
                return f(args);
            } else if constexpr (std::is_invocable_v<F>) {
                if constexpr (std::is_void_v<std::invoke_result_t<F>>) { f(); return var(); }
                else return f();
            } else if constexpr (std::is_invocable_v<F, var>) {
                var a1 = args.size() > 0 ? args[0] : var();
                if constexpr (std::is_void_v<std::invoke_result_t<F, var>>) { f(a1); return var(); }
                else return f(a1);
            } else if constexpr (std::is_invocable_v<F, var, var>) {
                var a1 = args.size() > 0 ? args[0] : var();
                var a2 = args.size() > 1 ? args[1] : var();
                if constexpr (std::is_void_v<std::invoke_result_t<F, var, var>>) { f(a1, a2); return var(); }
                else return f(a1, a2);
            } else {
                return var();
            }
        };
    }

    Type getType() const { return type; }
    bool isNull() const { return type == TYPE_NULL; }
    bool isString() const { return type == TYPE_STRING; }
    bool isFunction() const { return type == TYPE_FUNCTION; }
    bool isArray() const { return type == TYPE_ARRAY; }

    double toDouble() const {
        if (type == TYPE_DOUBLE) return double_val;
        if (type == TYPE_INT) return (double)int_val;
        if (type == TYPE_BOOL) return bool_val ? 1.0 : 0.0;
        if (type == TYPE_STRING) { char* end; double d = strtod(string_val.c_str(), &end); return d; }
        return 0.0;
    }
    long long toInt() const {
        if (type == TYPE_INT) return int_val;
        if (type == TYPE_DOUBLE) return (long long)double_val;
        if (type == TYPE_BOOL) return bool_val ? 1 : 0;
        if (type == TYPE_STRING) return strtoll(string_val.c_str(), nullptr, 10);
        return 0;
    }
    bool toBool() const {
        if (type == TYPE_BOOL) return bool_val;
        if (type == TYPE_INT) return int_val != 0;
        if (type == TYPE_DOUBLE) return double_val != 0.0;
        if (type == TYPE_STRING) return !string_val.empty() && string_val != "false" && string_val != "0";
        if (type == TYPE_ARRAY) return array_val && !array_val->empty();
        if (type == TYPE_OBJECT) return object_val && !object_val->empty();
        return false;
    }
    explicit operator bool() const { return toBool(); }
    bool as_bool() const { return toBool(); }

    var on(const var& event, const var& callback) {
        if (type == TYPE_OBJECT && object_val && object_val->count("on")) {
            return (*object_val)["on"](std::vector<var>{event, callback});
        }
        return var();
    }
    var get(const var& path, const var& callback) {
        if (type == TYPE_OBJECT && object_val && object_val->count("get")) {
            return (*object_val)["get"](std::vector<var>{path, callback});
        }
        return var();
    }
    var post(const var& path, const var& callback) {
        if (type == TYPE_OBJECT && object_val && object_val->count("post")) {
            return (*object_val)["post"](std::vector<var>{path, callback});
        }
        return var();
    }
    var listen(const var& port) {
        if (type == TYPE_OBJECT && object_val && object_val->count("listen")) {
            return (*object_val)["listen"](std::vector<var>{port});
        }
        return var();
    }
    var text(const var& content) {
        if (type == TYPE_OBJECT && object_val && object_val->count("text")) {
            return (*object_val)["text"](std::vector<var>{content});
        }
        return var();
    }
    var html(const var& content) {
        if (type == TYPE_OBJECT && object_val && object_val->count("html")) {
            return (*object_val)["html"](std::vector<var>{content});
        }
        return var();
    }
    var send(const var& content) {
        if (type == TYPE_OBJECT && object_val && object_val->count("send")) {
            return (*object_val)["send"](std::vector<var>{content});
        }
        return var();
    }
    var json(const var& content) {
        if (type == TYPE_OBJECT && object_val && object_val->count("json")) {
            return (*object_val)["json"](std::vector<var>{content});
        }
        return var();
    }

    std::string toString() const {
        if (type == TYPE_STRING) return string_val;
        if (type == TYPE_INT) return std::to_string(int_val);
        if (type == TYPE_DOUBLE) {
            std::string s = std::to_string(double_val);
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (!s.empty() && s.back() == '.') s.pop_back();
            return s;
        }
        if (type == TYPE_BOOL) return bool_val ? "true" : "false";
        if (type == TYPE_ARRAY) {
            std::string res = "[";
            if (array_val) {
                for (size_t i = 0; i < array_val->size(); ++i) {
                    res += (*array_val)[i].toString();
                    if (i + 1 < array_val->size()) res += ", ";
                }
            }
            return res + "]";
        }
        if (type == TYPE_FUNCTION) return "[Function]";
        return "null";
    }

    var operator()(const std::vector<var>& args = {}) const {
        if (type == TYPE_FUNCTION && func_val) return func_val(args);
        return var();
    }
    var operator()() const { return (*this)(std::vector<var>{}); }
    var operator()(const var& a1) const { return (*this)(std::vector<var>{a1}); }
    var operator()(const var& a1, const var& a2) const { return (*this)(std::vector<var>{a1, a2}); }

    void push(const var& v) {
        if (type != TYPE_ARRAY) { type = TYPE_ARRAY; array_val = std::make_shared<var_array>(); }
        array_val->push_back(v);
    }

    var length() const {
        if (type == TYPE_ARRAY && array_val) return var((long long)array_val->size());
        if (type == TYPE_STRING) return var((long long)string_val.length());
        return var(0);
    }

    var operator+(const var& o) const {
        if (type == TYPE_STRING || o.type == TYPE_STRING) return var(this->toString() + o.toString());
        if (type == TYPE_DOUBLE || o.type == TYPE_DOUBLE) return var(this->toDouble() + o.toDouble());
        return var(this->toInt() + o.toInt());
    }
    var operator-(const var& o) const {
        if (type == TYPE_DOUBLE || o.type == TYPE_DOUBLE) return var(this->toDouble() - o.toDouble());
        return var(this->toInt() - o.toInt());
    }
    var operator*(const var& o) const {
        if (type == TYPE_DOUBLE || o.type == TYPE_DOUBLE) return var(this->toDouble() * o.toDouble());
        return var(this->toInt() * o.toInt());
    }
    var operator/(const var& o) const {
        double denom = o.toDouble();
        if (denom == 0.0) return var(0.0);
        if (type == TYPE_DOUBLE || o.type == TYPE_DOUBLE) return var(this->toDouble() / denom);
        return var(this->toInt() / o.toInt());
    }
    var operator%(const var& o) const {
        long long denom = o.toInt();
        if (denom == 0) return var(0);
        return var(this->toInt() % denom);
    }
    var& operator+=(const var& o) { *this = *this + o; return *this; }
    var& operator-=(const var& o) { *this = *this - o; return *this; }
    var& operator*=(const var& o) { *this = *this * o; return *this; }
    var& operator/=(const var& o) { *this = *this / o; return *this; }
    var& operator%=(const var& o) { *this = *this % o; return *this; }
    var& operator++() { if (type == TYPE_DOUBLE) double_val++; else { type = TYPE_INT; int_val++; } return *this; }
    var operator++(int) { var t = *this; ++(*this); return t; }
    var& operator--() { if (type == TYPE_DOUBLE) double_val--; else { type = TYPE_INT; int_val--; } return *this; }
    var operator--(int) { var t = *this; --(*this); return t; }
    var operator!() const { return var(!toBool()); }
    var operator<<(const var& o) const { return var(this->toInt() << o.toInt()); }
    var operator>>(const var& o) const { return var(this->toInt() >> o.toInt()); }
    var operator&(const var& o) const { return var(this->toInt() & o.toInt()); }
    var operator|(const var& o) const { return var(this->toInt() | o.toInt()); }
    var operator^(const var& o) const { return var(this->toInt() ^ o.toInt()); }

    var operator==(const var& o) const {
        if (type == TYPE_STRING && o.type == TYPE_STRING) return var(string_val == o.string_val);
        if (type == TYPE_DOUBLE || o.type == TYPE_DOUBLE) return var(this->toDouble() == o.toDouble());
        return var(this->toInt() == o.toInt());
    }
    var operator!=(const var& o) const { return var(!((*this == o).toBool())); }
    var operator<(const var& o) const {
        if (type == TYPE_STRING && o.type == TYPE_STRING) return var(string_val < o.string_val);
        return var(this->toDouble() < o.toDouble());
    }
    var operator>(const var& o) const {
        if (type == TYPE_STRING && o.type == TYPE_STRING) return var(string_val > o.string_val);
        return var(this->toDouble() > o.toDouble());
    }
    var operator<=(const var& o) const { return var(!((*this > o).toBool())); }
    var operator>=(const var& o) const { return var(!((*this < o).toBool())); }

    var& operator[](const std::string& key) {
        if (type != TYPE_OBJECT) {
            type = TYPE_OBJECT;
            object_val = std::make_shared<var_object>();
        }
        return (*object_val)[key];
    }
    const var& operator[](const std::string& key) const {
        static const var empty;
        if (type == TYPE_OBJECT && object_val) {
            auto found = object_val->find(key);
            if (found != object_val->end()) return found->second;
        }
        return empty;
    }
    var& operator[](const char* key) {
        return (*this)[std::string(key)];
    }
    const var& operator[](const char* key) const {
        return (*this)[std::string(key)];
    }
    var& operator[](const var& key) {
        return (*this)[key.toString()];
    }
    const var& operator[](const var& key) const {
        return (*this)[key.toString()];
    }
};

inline var operator+(const char* lhs, const var& rhs) { return var(lhs) + rhs; }
inline var operator+(const std::string& lhs, const var& rhs) { return var(lhs) + rhs; }
inline var operator+(double lhs, const var& rhs) { return var(lhs) + rhs; }
inline var operator+(int lhs, const var& rhs) { return var(lhs) + rhs; }

inline void print() { Serial.println(); }

template<typename T, typename... Args>
inline void print(T first, Args... args) {
    Serial.print(var(first).toString().c_str());
    if constexpr (sizeof...(args) > 0) {
        Serial.print(" ");
        print(args...);
    } else {
        Serial.println();
    }
}

inline var input(const std::string& prompt = "") {
    if (!prompt.empty()) Serial.print(prompt.c_str());
    while (!Serial.available()) { delay(10); }
    std::string s = Serial.readStringUntil('\n').c_str();
    return var(s);
}

// Real GPIO pin, matching the Pin API used by dolphin_runtime.hpp's PC
// simulation (pin(num, mode), .write(), .read(), .on("change", cb)).
// NOTE: HIGH/LOW/INPUT/OUTPUT are intentionally NOT redefined here -- they
// come from Arduino.h's own macros. Codegen already rewrites the dolphin
// keywords `INPUT`/`OUTPUT` to `PIN_INPUT`/`PIN_OUTPUT` (this file's enum
// below), while `HIGH`/`LOW` pass through untouched and resolve to
// Arduino.h's macros at hardware-compile time.
enum PinMode { PIN_INPUT, PIN_OUTPUT };
#define DOLPHIN_INPUT   PIN_INPUT
#define DOLPHIN_OUTPUT  PIN_OUTPUT
#define DOLPHIN_HIGH    HIGH
#define DOLPHIN_LOW     LOW

class Pin;

namespace DolphinRuntime {
    inline std::vector<::Pin*>& pollRegistry();
}

class Pin {
private:
    int pin_num;
    PinMode mode;
    bool state = false;
    std::vector<var> listeners;

public:
    Pin() : pin_num(-1), mode(PIN_INPUT) {}
    Pin(int p, PinMode m) : pin_num(p), mode(m) {
        pinMode(pin_num, m == PIN_OUTPUT ? OUTPUT : INPUT);
        if (m == PIN_INPUT) state = digitalRead(pin_num) == HIGH;
    }
    Pin(const std::vector<var>& args) {
        pin_num = args.size() > 0 ? args[0].toInt() : -1;
        mode = args.size() > 1 ? (PinMode)args[1].toInt() : PIN_INPUT;
        pinMode(pin_num, mode == PIN_OUTPUT ? OUTPUT : INPUT);
        if (mode == PIN_INPUT) state = digitalRead(pin_num) == HIGH;
    }

    void write(const var& s) {
        state = s.toBool();
        if (mode == PIN_OUTPUT) digitalWrite(pin_num, state ? HIGH : LOW);
        trigger(var(state));
    }

    var read() const {
        if (mode == PIN_INPUT) return var(digitalRead(pin_num) == HIGH);
        return var(state);
    }

    void on(const var& event, const var& callback) {
        if (event.toString() == "change") {
            listeners.push_back(callback);
            DolphinRuntime::pollRegistry().push_back(static_cast<::Pin*>(this));
        }
    }

    var operator[](const std::string& key) {
        if (key == "write") {
            return var([this](const std::vector<var>& args) -> var {
                if (args.size() > 0) this->write(args[0]);
                return var();
            });
        }
        if (key == "read") {
            return var([this](const std::vector<var>& args) -> var {
                return this->read();
            });
        }
        return var();
    }

    // Called every loop() iteration by DolphinRuntime::pollPins() so
    // input-pin `.on("change", ...)` listeners fire on real hardware,
    // since there is no PC-style manual event trigger on a board.
    void poll() {
        if (mode != PIN_INPUT) return;
        bool current = digitalRead(pin_num) == HIGH;
        if (current != state) {
            state = current;
            trigger(var(state));
        }
    }

private:
    void trigger(var value) {
        for (auto& cb : listeners) cb(std::vector<var>{value});
    }
};

using pin = Pin;

namespace DolphinRuntime {
    inline std::vector<Pin*>& pollRegistry() {
        static std::vector<Pin*> registry;
        return registry;
    }
    inline void pollPins() {
        for (Pin* p : pollRegistry()) p->poll();
        if (global_esp_web_server) {
            global_esp_web_server->handleClient();
        }
    }
}

struct MathClass {
    var random() { return var((double)rand() / RAND_MAX); }
    var floor(const var& v) { return var(std::floor(v.toDouble())); }
    var ceil(const var& v) { return var(std::ceil(v.toDouble())); }
    var round(const var& v) { return var(std::round(v.toDouble())); }
    var abs(const var& v) { return var(std::abs(v.toDouble())); }
    var sin(const var& v) { return var(std::sin(v.toDouble())); }
    var cos(const var& v) { return var(std::cos(v.toDouble())); }
    var pow(const var& base, const var& exp) { return var(std::pow(base.toDouble(), exp.toDouble())); }
    var sqrt(const var& v) { return var(std::sqrt(v.toDouble())); }
    var PI = var(3.14159265358979323846);
} Math;

struct JSONClass {
    std::string stringify(const var& v) { return v.toString(); }
} JSON;

inline var sleep(const std::vector<var>& args) {
    if (args.size() > 0) {
        unsigned long start = millis();
        unsigned long dur = args[0].toInt();
        while (millis() - start < dur) {
            if (global_esp_web_server) {
                global_esp_web_server->handleClient();
            }
            delay(1);
        }
    }
    return var();
}
inline var sleep(const var& ms) {
    unsigned long start = millis();
    unsigned long dur = ms.toInt();
    while (millis() - start < dur) {
        if (global_esp_web_server) {
            global_esp_web_server->handleClient();
        }
        delay(1);
    }
    return var();
}

struct WifiClass {
    var connect(const var& ssid, const var& password) {
        ::WiFi.begin(ssid.toString().c_str(), password.toString().c_str());
        while (::WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        Serial.println("\nWiFi connected");
        return var(true);
    }
    var softAP(const var& ssid, const var& password) {
        ::WiFi.softAP(ssid.toString().c_str(), password.toString().c_str());
        Serial.print("Access Point started. IP: ");
        Serial.println(::WiFi.softAPIP());
        return var(::WiFi.softAPIP().toString().c_str());
    }
    var ip() {
        return var(::WiFi.localIP().toString().c_str());
    }
    var status() {
        return var((long long)::WiFi.status());
    }
    var operator[](const std::string& key) {
        if (key == "softAP") {
            return var([this](const std::vector<var>& args) -> var {
                if (args.size() > 1) return this->softAP(args[0], args[1]);
                return var();
            });
        }
        if (key == "connect") {
            return var([this](const std::vector<var>& args) -> var {
                if (args.size() > 1) return this->connect(args[0], args[1]);
                return var();
            });
        }
        if (key == "ip") {
            return var([this](const std::vector<var>& args) -> var {
                return this->ip();
            });
        }
        if (key == "status") {
            return var([this](const std::vector<var>& args) -> var {
                return this->status();
            });
        }
        return var();
    }
} DolphinWifi;

struct HTTPRouteHandler {
    std::string path;
    var callback;
};

static std::vector<HTTPRouteHandler> global_get_routes;
static std::vector<HTTPRouteHandler> global_post_routes;

class HTTPServerClass {
public:
    void get(const var& path, const var& callback) {
        global_get_routes.push_back({path.toString(), callback});
    }
    void post(const var& path, const var& callback) {
        global_post_routes.push_back({path.toString(), callback});
    }
    void listen(const var& port) {
        int port_num = port.toInt();
        static WebServerClass server(port_num);
        global_esp_web_server = &server;

        for (const auto& r : global_get_routes) {
            std::string path = r.path;
            var cb = r.callback;
            server.on(path.c_str(), HTTP_GET, [cb]() {
                var req(var::TYPE_OBJECT);
                var params(var::TYPE_OBJECT);
                if (global_esp_web_server) {
                    for (int i = 0; i < global_esp_web_server->args(); i++) {
                        params[global_esp_web_server->argName(i).c_str()] = var(global_esp_web_server->arg(i).c_str());
                    }
                }
                req["params"] = params;

                var res(var::TYPE_OBJECT);
                res["text"] = var([](const std::vector<var>& args) -> var {
                    if (args.size() > 0 && global_esp_web_server) {
                        global_esp_web_server->send(200, "text/plain", args[0].toString().c_str());
                    }
                    return var();
                });
                res["html"] = var([](const std::vector<var>& args) -> var {
                    if (args.size() > 0 && global_esp_web_server) {
                        global_esp_web_server->send(200, "text/html", args[0].toString().c_str());
                    }
                    return var();
                });
                res["send"] = var([](const std::vector<var>& args) -> var {
                    if (args.size() > 0 && global_esp_web_server) {
                        std::string content = args[0].toString();
                        std::string type = "text/plain";
                        if (content.find("<html") != std::string::npos || content.find("<HTML") != std::string::npos || content.find("<!DOCTYPE") != std::string::npos) {
                            type = "text/html";
                        }
                        global_esp_web_server->send(200, type.c_str(), content.c_str());
                    }
                    return var();
                });
                res["json"] = var([](const std::vector<var>& args) -> var {
                    if (args.size() > 0 && global_esp_web_server) {
                        global_esp_web_server->send(200, "application/json", args[0].toString().c_str());
                    }
                    return var();
                });

                cb(std::vector<var>{req, res});
            });
        }

        for (const auto& r : global_post_routes) {
            std::string path = r.path;
            var cb = r.callback;
            server.on(path.c_str(), HTTP_POST, [cb]() {
                var req(var::TYPE_OBJECT);
                var params(var::TYPE_OBJECT);
                if (global_esp_web_server) {
                    for (int i = 0; i < global_esp_web_server->args(); i++) {
                        params[global_esp_web_server->argName(i).c_str()] = var(global_esp_web_server->arg(i).c_str());
                    }
                }
                req["params"] = params;

                var res(var::TYPE_OBJECT);
                res["text"] = var([](const std::vector<var>& args) -> var {
                    if (args.size() > 0 && global_esp_web_server) {
                        global_esp_web_server->send(200, "text/plain", args[0].toString().c_str());
                    }
                    return var();
                });
                res["html"] = var([](const std::vector<var>& args) -> var {
                    if (args.size() > 0 && global_esp_web_server) {
                        global_esp_web_server->send(200, "text/html", args[0].toString().c_str());
                    }
                    return var();
                });
                res["send"] = var([](const std::vector<var>& args) -> var {
                    if (args.size() > 0 && global_esp_web_server) {
                        std::string content = args[0].toString();
                        std::string type = "text/plain";
                        if (content.find("<html") != std::string::npos || content.find("<HTML") != std::string::npos || content.find("<!DOCTYPE") != std::string::npos) {
                            type = "text/html";
                        }
                        global_esp_web_server->send(200, type.c_str(), content.c_str());
                    }
                    return var();
                });
                res["json"] = var([](const std::vector<var>& args) -> var {
                    if (args.size() > 0 && global_esp_web_server) {
                        global_esp_web_server->send(200, "application/json", args[0].toString().c_str());
                    }
                    return var();
                });

                cb(std::vector<var>{req, res});
            });
        }

        server.onNotFound([]() {
            if (global_esp_web_server) {
                global_esp_web_server->send(404, "text/plain", "Not Found");
            }
        });

        server.begin();
        Serial.println("HTTP Server started");
    }
};

struct HTTPNamespace {
    var Server() {
        auto server_ptr = std::make_shared<HTTPServerClass>();
        var s(var::TYPE_OBJECT);
        s["get"] = var([server_ptr](const std::vector<var>& args) -> var {
            if (args.size() > 1) server_ptr->get(args[0], args[1]);
            return var();
        });
        s["post"] = var([server_ptr](const std::vector<var>& args) -> var {
            if (args.size() > 1) server_ptr->post(args[0], args[1]);
            return var();
        });
        s["listen"] = var([server_ptr](const std::vector<var>& args) -> var {
            if (args.size() > 0) server_ptr->listen(args[0]);
            return var();
        });
        return s;
    }
} HTTP;

template<typename... Args>
inline void dolphin_print(Args... args) {
    print(args...);
}

template<typename... Args>
inline void dolphin_println(Args... args) {
    print(args...);
}
