#pragma once
#include <iostream>
#include <fstream>
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
#include <ctime>
#include <type_traits>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <atomic>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    using SOCKET = int;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket ::close
#endif

class HTTPServerClass;
class TCPServerClass;
class TCPSocketClass;

class var;
using var_array = std::vector<var>;
using var_object = std::map<std::string, var>;

class var {
public:
    enum Type {
        TYPE_NULL,
        TYPE_BOOL,
        TYPE_INT,
        TYPE_DOUBLE,
        TYPE_STRING,
        TYPE_ARRAY,
        TYPE_OBJECT,
        TYPE_FUNCTION
    };

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
    std::shared_ptr<std::map<std::string, std::vector<var>>> event_listeners = nullptr;
    std::shared_ptr<HTTPServerClass> http_server = nullptr;
    std::shared_ptr<TCPServerClass> tcp_server = nullptr;
    std::shared_ptr<TCPSocketClass> tcp_socket = nullptr;
    int http_status = 200;

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
    var(std::initializer_list<std::pair<const std::string, var>> list) : type(TYPE_OBJECT), object_val(std::make_shared<var_object>(list.begin(), list.end())) {}

    template<typename F, typename = std::enable_if_t<
        !std::is_same_v<std::decay_t<F>, var> && 
        (std::is_invocable_v<F> || 
         std::is_invocable_v<F, var> || 
         std::is_invocable_v<F, var, var> || 
         std::is_invocable_v<F, var, var, var> ||
         std::is_invocable_v<F, const std::vector<var>&>)
    >>
    var(F&& f) : type(TYPE_FUNCTION) {
        func_val = [f = std::forward<F>(f)](const std::vector<var>& args) mutable -> var {
            if constexpr (std::is_invocable_r_v<var, F, const std::vector<var>&>) {
                return f(args);
            } else if constexpr (std::is_invocable_v<F>) {
                if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
                    f();
                    return var();
                } else {
                    return f();
                }
            } else if constexpr (std::is_invocable_v<F, var>) {
                var a1 = args.size() > 0 ? args[0] : var();
                if constexpr (std::is_void_v<std::invoke_result_t<F, var>>) {
                    f(a1);
                    return var();
                } else {
                    return f(a1);
                }
            } else if constexpr (std::is_invocable_v<F, var, var>) {
                var a1 = args.size() > 0 ? args[0] : var();
                var a2 = args.size() > 1 ? args[1] : var();
                if constexpr (std::is_void_v<std::invoke_result_t<F, var, var>>) {
                    f(a1, a2);
                    return var();
                } else {
                    return f(a1, a2);
                }
            } else if constexpr (std::is_invocable_v<F, var, var, var>) {
                var a1 = args.size() > 0 ? args[0] : var();
                var a2 = args.size() > 1 ? args[1] : var();
                var a3 = args.size() > 2 ? args[2] : var();
                if constexpr (std::is_void_v<std::invoke_result_t<F, var, var, var>>) {
                    f(a1, a2, a3);
                    return var();
                } else {
                    return f(a1, a2, a3);
                }
            } else {
                return var();
            }
        };
    }

    Type getType() const { return type; }

    bool isNull() const { return type == TYPE_NULL; }
    bool isBool() const { return type == TYPE_BOOL; }
    bool isInt() const { return type == TYPE_INT; }
    bool isDouble() const { return type == TYPE_DOUBLE; }
    bool isString() const { return type == TYPE_STRING; }
    bool isArray() const { return type == TYPE_ARRAY; }
    bool isObject() const { return type == TYPE_OBJECT; }
    bool isFunction() const { return type == TYPE_FUNCTION; }

    double toDouble() const {
        if (type == TYPE_DOUBLE) return double_val;
        if (type == TYPE_INT) return (double)int_val;
        if (type == TYPE_BOOL) return bool_val ? 1.0 : 0.0;
        if (type == TYPE_STRING) {
            try { return std::stod(string_val); } catch(...) { return 0.0; }
        }
        return 0.0;
    }

    long long toInt() const {
        if (type == TYPE_INT) return int_val;
        if (type == TYPE_DOUBLE) return (long long)double_val;
        if (type == TYPE_BOOL) return bool_val ? 1 : 0;
        if (type == TYPE_STRING) {
            try { return std::stoll(string_val); } catch(...) { return 0; }
        }
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

    explicit operator bool() const {
        return toBool();
    }

    std::string toString() const {
        if (type == TYPE_STRING) return string_val;
        if (type == TYPE_INT) return std::to_string(int_val);
        if (type == TYPE_DOUBLE) {
            std::string s = std::to_string(double_val);
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.') s.pop_back();
            return s;
        }
        if (type == TYPE_BOOL) return bool_val ? "true" : "false";
        if (type == TYPE_ARRAY) {
            std::string res = "[";
            if (array_val) {
                for (size_t i = 0; i < array_val->size(); ++i) {
                    const var& val = (*array_val)[i];
                    if (val.isString()) {
                        res += "\"" + val.toString() + "\"";
                    } else {
                        res += val.toString();
                    }
                    if (i + 1 < array_val->size()) res += ", ";
                }
            }
            res += "]";
            return res;
        }
        if (type == TYPE_OBJECT) {
            std::string res = "{";
            if (object_val) {
                size_t i = 0;
                for (auto const& [key, val] : *object_val) {
                    res += "\"" + key + "\": ";
                    if (val.isString()) {
                        res += "\"" + val.toString() + "\"";
                    } else {
                        res += val.toString();
                    }
                    if (++i < object_val->size()) res += ", ";
                }
            }
            res += "}";
            return res;
        }
        if (type == TYPE_FUNCTION) {
            return "[Function]";
        }
        return "null";
    }

    var operator()(const std::vector<var>& args = {}) const {
        if (type == TYPE_FUNCTION && func_val) {
            return func_val(args);
        }
        return var();
    }

    var operator()() const {
        return (*this)(std::vector<var>{});
    }

    var operator()(const var& a1) const {
        return (*this)(std::vector<var>{a1});
    }

    var operator()(const var& a1, const var& a2) const {
        return (*this)(std::vector<var>{a1, a2});
    }

    var operator()(const var& a1, const var& a2, const var& a3) const {
        return (*this)(std::vector<var>{a1, a2, a3});
    }

    void add(const var& v) {
        if (type != TYPE_ARRAY) {
            type = TYPE_ARRAY;
            array_val = std::make_shared<var_array>();
        }
        array_val->push_back(v);
    }

    var& operator[](size_t index) {
        if (type != TYPE_ARRAY || !array_val) {
            type = TYPE_ARRAY;
            array_val = std::make_shared<var_array>();
        }
        if (index >= array_val->size()) {
            array_val->resize(index + 1);
        }
        return (*array_val)[index];
    }

    const var& operator[](size_t index) const {
        static const var null_var;
        if (type != TYPE_ARRAY || !array_val || index >= array_val->size()) {
            return null_var;
        }
        return (*array_val)[index];
    }

    var& operator[](int index) {
        return (*this)[(size_t)index];
    }

    const var& operator[](int index) const {
        return (*this)[(size_t)index];
    }

    var& operator[](const std::string& key) {
        if (type != TYPE_OBJECT || !object_val) {
            type = TYPE_OBJECT;
            object_val = std::make_shared<var_object>();
        }
        return (*object_val)[key];
    }

    const var& operator[](const std::string& key) const {
        static const var null_var;
        if (type != TYPE_OBJECT || !object_val) return null_var;
        auto it = object_val->find(key);
        if (it == object_val->end()) return null_var;
        return it->second;
    }

    var& operator[](const char* key) {
        return (*this)[std::string(key)];
    }

    const var& operator[](const char* key) const {
        return (*this)[std::string(key)];
    }

    var& operator[](const var& key) {
        if (key.type == TYPE_INT) {
            return (*this)[(int)key.toInt()];
        }
        return (*this)[key.toString()];
    }

    const var& operator[](const var& key) const {
        if (key.type == TYPE_INT) {
            return (*this)[(int)key.toInt()];
        }
        return (*this)[key.toString()];
    }

    // Array / String / Object methods
    void push(const var& v) {
        add(v);
    }

    var pop() {
        if (type == TYPE_ARRAY && array_val && !array_val->empty()) {
            var back = array_val->back();
            array_val->pop_back();
            return back;
        }
        return var();
    }

    var shift() {
        if (type == TYPE_ARRAY && array_val && !array_val->empty()) {
            var front = array_val->front();
            array_val->erase(array_val->begin());
            return front;
        }
        return var();
    }

    void unshift(const var& v) {
        if (type != TYPE_ARRAY) {
            type = TYPE_ARRAY;
            array_val = std::make_shared<var_array>();
        }
        array_val->insert(array_val->begin(), v);
    }

    var length() const {
        if (type == TYPE_ARRAY && array_val) return var((long long)array_val->size());
        if (type == TYPE_STRING) return var((long long)string_val.length());
        if (type == TYPE_OBJECT && object_val) return var((long long)object_val->size());
        return var(0);
    }

    var join(const std::string& separator = ",") const {
        if (type != TYPE_ARRAY || !array_val) return var("");
        std::string result = "";
        for (size_t i = 0; i < array_val->size(); ++i) {
            result += (*array_val)[i].toString();
            if (i + 1 < array_val->size()) result += separator;
        }
        return var(result);
    }

    var split(const std::string& separator) const {
        var res;
        if (type != TYPE_STRING) return res;
        std::string s = string_val;
        size_t pos = 0;
        std::string token;
        if (separator.empty()) {
            for (char c : s) {
                res.push(var(std::string(1, c)));
            }
            return res;
        }
        while ((pos = s.find(separator)) != std::string::npos) {
            token = s.substr(0, pos);
            res.push(var(token));
            s.erase(0, pos + separator.length());
        }
        res.push(var(s));
        return res;
    }

    var indexOf(const var& search) const {
        if (type == TYPE_STRING) {
            size_t pos = string_val.find(search.toString());
            return pos == std::string::npos ? var(-1) : var((long long)pos);
        }
        if (type == TYPE_ARRAY && array_val) {
            for (size_t i = 0; i < array_val->size(); ++i) {
                if ((*array_val)[i] == search) return var((long long)i);
            }
        }
        return var(-1);
    }

    var toLowerCase() const {
        if (type != TYPE_STRING) return *this;
        std::string s = string_val;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
        return var(s);
    }

    var toUpperCase() const {
        if (type != TYPE_STRING) return *this;
        std::string s = string_val;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
        return var(s);
    }

    var trim() const {
        if (type == TYPE_STRING) {
            std::string str = string_val;
            size_t first = str.find_first_not_of(" \t\r\n");
            if (std::string::npos == first) {
                return var("");
            }
            size_t last = str.find_last_not_of(" \t\r\n");
            return var(str.substr(first, (last - first + 1)));
        }
        return *this;
    }

    var keys() const {
        var res;
        if (type == TYPE_OBJECT && object_val) {
            for (auto const& [key, val] : *object_val) {
                res.push(var(key));
            }
        }
        return res;
    }

    var has(const std::string& key) const {
        if (type == TYPE_OBJECT && object_val) {
            return var(object_val->count(key) > 0);
        }
        return var(false);
    }

    var has(const var& key) const {
        return has(key.toString());
    }

    var on(const std::string& event, const var& callback) {
        if (!event_listeners) {
            event_listeners = std::make_shared<std::map<std::string, std::vector<var>>>();
        }
        (*event_listeners)[event].push_back(callback);
        return *this;
    }

    var emit(const std::string& event, const var& data = var()) {
        if (event_listeners && event_listeners->count(event)) {
            std::vector<var> current_listeners = (*event_listeners)[event];
            for (const auto& cb : current_listeners) {
                if (cb.isFunction()) {
                    cb(std::vector<var>{data});
                }
            }
        }
        return *this;
    }

    var off(const std::string& event) {
        if (event_listeners) {
            event_listeners->erase(event);
        }
        return *this;
    }

    var once(const std::string& event, const var& callback) {
        auto fired = std::make_shared<bool>(false);
        var wrapper;
        wrapper = var([fired, callback](const std::vector<var>& args) -> var {
            if (!(*fired)) {
                *fired = true;
                if (callback.isFunction()) {
                    callback(args);
                }
            }
            return var();
        });
        on(event, wrapper);
        return *this;
    }

    // JS-like Functional Array methods
    template<typename F>
    var map(F callback) const {
        var res;
        if (type == TYPE_ARRAY && array_val) {
            for (size_t i = 0; i < array_val->size(); ++i) {
                if constexpr (std::is_invocable_v<F, var, var>) {
                    res.push(var(callback((*array_val)[i], var((long long)i))));
                } else {
                    res.push(var(callback((*array_val)[i])));
                }
            }
        }
        return res;
    }

    template<typename F>
    var find(F callback) const {
        if (type == TYPE_ARRAY && array_val) {
            for (size_t i = 0; i < array_val->size(); ++i) {
                if constexpr (std::is_invocable_v<F, var, var>) {
                    if (var(callback((*array_val)[i], var((long long)i))).toBool()) {
                        return (*array_val)[i];
                    }
                } else {
                    if (var(callback((*array_val)[i])).toBool()) {
                        return (*array_val)[i];
                    }
                }
            }
        }
        return var();
    }

    template<typename F>
    var findOne(F callback) const {
        return find(callback);
    }

    template<typename F>
    var filter(F callback) const {
        var res;
        if (type == TYPE_ARRAY && array_val) {
            for (size_t i = 0; i < array_val->size(); ++i) {
                if constexpr (std::is_invocable_v<F, var, var>) {
                    if (var(callback((*array_val)[i], var((long long)i))).toBool()) {
                        res.push((*array_val)[i]);
                    }
                } else {
                    if (var(callback((*array_val)[i])).toBool()) {
                        res.push((*array_val)[i]);
                    }
                }
            }
        }
        return res;
    }

    template<typename F>
    void forEach(F callback) const {
        if (type == TYPE_ARRAY && array_val) {
            for (size_t i = 0; i < array_val->size(); ++i) {
                if constexpr (std::is_invocable_v<F, var, var>) {
                    callback((*array_val)[i], var((long long)i));
                } else {
                    callback((*array_val)[i]);
                }
            }
        }
    }

    std::vector<var>::iterator begin() {
        if (type != TYPE_ARRAY || !array_val) {
            static std::vector<var> empty;
            return empty.begin();
        }
        return array_val->begin();
    }
    std::vector<var>::iterator end() {
        if (type != TYPE_ARRAY || !array_val) {
            static std::vector<var> empty;
            return empty.end();
        }
        return array_val->end();
    }
    std::vector<var>::const_iterator begin() const {
        if (type != TYPE_ARRAY || !array_val) {
            static const std::vector<var> empty;
            return empty.begin();
        }
        return array_val->begin();
    }
    std::vector<var>::const_iterator end() const {
        if (type != TYPE_ARRAY || !array_val) {
            static const std::vector<var> empty;
            return empty.end();
        }
        return array_val->end();
    }

    var isOdd() const {
        if (type == TYPE_INT) return var(int_val % 2 != 0);
        if (type == TYPE_DOUBLE) return var((long long)double_val % 2 != 0);
        return var(false);
    }
    var isEven() const {
        if (type == TYPE_INT) return var(int_val % 2 == 0);
        if (type == TYPE_DOUBLE) return var((long long)double_val % 2 == 0);
        return var(false);
    }

    var values() const {
        var res;
        if (type == TYPE_OBJECT && object_val) {
            for (const auto& pair : *object_val) {
                res.push(pair.second);
            }
        }
        return res;
    }
    var entries() const {
        var res;
        if (type == TYPE_OBJECT && object_val) {
            for (const auto& pair : *object_val) {
                var entry;
                entry.push(var(pair.first));
                entry.push(pair.second);
                res.push(entry);
            }
        }
        return res;
    }

    var rotateRight(const var& steps_var = var(1)) const {
        long long steps = steps_var.toInt();
        if (type == TYPE_ARRAY && array_val && !array_val->empty()) {
            long long size = array_val->size();
            steps = (steps % size + size) % size;
            var res;
            for (long long i = 0; i < size; ++i) {
                res.push((*array_val)[(i - steps + size) % size]);
            }
            return res;
        }
        if (type == TYPE_STRING) {
            if (string_val.empty()) return *this;
            long long size = string_val.length();
            steps = (steps % size + size) % size;
            return var(string_val.substr(size - steps) + string_val.substr(0, size - steps));
        }
        return *this;
    }

    var rotateLeft(const var& steps_var = var(1)) const {
        long long steps = steps_var.toInt();
        if (type == TYPE_ARRAY && array_val && !array_val->empty()) {
            long long size = array_val->size();
            steps = (steps % size + size) % size;
            var res;
            for (long long i = 0; i < size; ++i) {
                res.push((*array_val)[(i + steps) % size]);
            }
            return res;
        }
        if (type == TYPE_STRING) {
            if (string_val.empty()) return *this;
            long long size = string_val.length();
            steps = (steps % size + size) % size;
            return var(string_val.substr(steps) + string_val.substr(0, steps));
        }
        return *this;
    }

    var operator+(const var& other) const {
        if (type == TYPE_STRING || other.type == TYPE_STRING) {
            return var(this->toString() + other.toString());
        }
        if (type == TYPE_DOUBLE || other.type == TYPE_DOUBLE) {
            return var(this->toDouble() + other.toDouble());
        }
        return var(this->toInt() + other.toInt());
    }

    var operator-(const var& other) const {
        if (type == TYPE_DOUBLE || other.type == TYPE_DOUBLE) {
            return var(this->toDouble() - other.toDouble());
        }
        return var(this->toInt() - other.toInt());
    }

    var operator*(const var& other) const {
        if (type == TYPE_DOUBLE || other.type == TYPE_DOUBLE) {
            return var(this->toDouble() * other.toDouble());
        }
        return var(this->toInt() * other.toInt());
    }

    var operator/(const var& other) const {
        double denom = other.toDouble();
        if (denom == 0.0) return var(0.0);
        if (type == TYPE_DOUBLE || other.type == TYPE_DOUBLE) {
            return var(this->toDouble() / denom);
        }
        return var(this->toInt() / other.toInt());
    }

    var operator%(const var& other) const {
        long long denom = other.toInt();
        if (denom == 0) return var(0);
        return var(this->toInt() % denom);
    }

    var& operator+=(const var& other) {
        *this = *this + other;
        return *this;
    }
    var& operator-=(const var& other) {
        *this = *this - other;
        return *this;
    }
    var& operator*=(const var& other) {
        *this = *this * other;
        return *this;
    }
    var& operator/=(const var& other) {
        *this = *this / other;
        return *this;
    }
    var& operator%=(const var& other) {
        *this = *this % other;
        return *this;
    }

    // Prefix increment
    var& operator++() {
        if (type == TYPE_DOUBLE) double_val++;
        else {
            type = TYPE_INT;
            int_val++;
        }
        return *this;
    }
    // Postfix increment
    var operator++(int) {
        var temp = *this;
        ++(*this);
        return temp;
    }
    // Prefix decrement
    var& operator--() {
        if (type == TYPE_DOUBLE) double_val--;
        else {
            type = TYPE_INT;
            int_val--;
        }
        return *this;
    }
    // Postfix decrement
    var operator--(int) {
        var temp = *this;
        --(*this);
        return temp;
    }

    var operator!() const {
        return var(!toBool());
    }

    var operator<<(const var& other) const {
        return var(this->toInt() << other.toInt());
    }
    var operator>>(const var& other) const {
        return var(this->toInt() >> other.toInt());
    }
    var operator&(const var& other) const {
        return var(this->toInt() & other.toInt());
    }
    var operator|(const var& other) const {
        return var(this->toInt() | other.toInt());
    }
    var operator^(const var& other) const {
        return var(this->toInt() ^ other.toInt());
    }
    var operator~() const {
        return var(~this->toInt());
    }

    var& operator<<=(const var& other) { *this = *this << other; return *this; }
    var& operator>>=(const var& other) { *this = *this >> other; return *this; }
    var& operator&=(const var& other) { *this = *this & other; return *this; }
    var& operator|=(const var& other) { *this = *this | other; return *this; }
    var& operator^=(const var& other) { *this = *this ^ other; return *this; }

    var operator==(const var& other) const {
        if (type == TYPE_STRING && other.type == TYPE_STRING) return var(string_val == other.string_val);
        if (type == TYPE_DOUBLE || other.type == TYPE_DOUBLE) return var(this->toDouble() == other.toDouble());
        return var(this->toInt() == other.toInt());
    }

    var operator!=(const var& other) const {
        return var(!((*this == other).toBool()));
    }

    var operator<(const var& other) const {
        if (type == TYPE_STRING && other.type == TYPE_STRING) return var(string_val < other.string_val);
        if (type == TYPE_DOUBLE || other.type == TYPE_DOUBLE) return var(this->toDouble() < other.toDouble());
        return var(this->toInt() < other.toInt());
    }

    var operator>(const var& other) const {
        if (type == TYPE_STRING && other.type == TYPE_STRING) return var(string_val > other.string_val);
        if (type == TYPE_DOUBLE || other.type == TYPE_DOUBLE) return var(this->toDouble() > other.toDouble());
        return var(this->toInt() > other.toInt());
    }

    var operator<=(const var& other) const {
        return var(!((*this > other).toBool()));
    }

    var operator>=(const var& other) const {
        return var(!((*this < other).toBool()));
    }

    var get(const var& path, const var& cb);
    var post(const var& path, const var& cb);
    var listen(const var& port);
    var write(const var& data);
    var close();
    var status(const var& code);
    var send(const var& body);
    var json(const var& body);
};

inline std::ostream& operator<<(std::ostream& os, const var& v) {
    os << v.toString();
    return os;
}

// Global operator overloads for LHS values
inline var operator+(const char* lhs, const var& rhs) { return var(lhs) + rhs; }
inline var operator+(const std::string& lhs, const var& rhs) { return var(lhs) + rhs; }
inline var operator+(double lhs, const var& rhs) { return var(lhs) + rhs; }
inline var operator+(long long lhs, const var& rhs) { return var(lhs) + rhs; }
inline var operator+(int lhs, const var& rhs) { return var(lhs) + rhs; }
inline var operator+(bool lhs, const var& rhs) { return var(lhs) + rhs; }

inline var operator-(double lhs, const var& rhs) { return var(lhs) - rhs; }
inline var operator-(long long lhs, const var& rhs) { return var(lhs) - rhs; }
inline var operator-(int lhs, const var& rhs) { return var(lhs) - rhs; }

inline var operator*(double lhs, const var& rhs) { return var(lhs) * rhs; }
inline var operator*(long long lhs, const var& rhs) { return var(lhs) * rhs; }
inline var operator*(int lhs, const var& rhs) { return var(lhs) * rhs; }

inline var operator/(double lhs, const var& rhs) { return var(lhs) / rhs; }
inline var operator/(long long lhs, const var& rhs) { return var(lhs) / rhs; }
inline var operator/(int lhs, const var& rhs) { return var(lhs) / rhs; }

inline var operator%(long long lhs, const var& rhs) { return var(lhs) % rhs; }
inline var operator%(int lhs, const var& rhs) { return var(lhs) % rhs; }

inline var operator==(const char* lhs, const var& rhs) { return var(lhs) == rhs; }
inline var operator==(const std::string& lhs, const var& rhs) { return var(lhs) == rhs; }
inline var operator==(double lhs, const var& rhs) { return var(lhs) == rhs; }
inline var operator==(long long lhs, const var& rhs) { return var(lhs) == rhs; }
inline var operator==(int lhs, const var& rhs) { return var(lhs) == rhs; }
inline var operator==(bool lhs, const var& rhs) { return var(lhs) == rhs; }

inline var operator!=(const char* lhs, const var& rhs) { return var(lhs) != rhs; }
inline var operator!=(const std::string& lhs, const var& rhs) { return var(lhs) != rhs; }
inline var operator!=(double lhs, const var& rhs) { return var(lhs) != rhs; }
inline var operator!=(long long lhs, const var& rhs) { return var(lhs) != rhs; }
inline var operator!=(int lhs, const var& rhs) { return var(lhs) != rhs; }
inline var operator!=(bool lhs, const var& rhs) { return var(lhs) != rhs; }

inline var operator<(const char* lhs, const var& rhs) { return var(lhs) < rhs; }
inline var operator<(const std::string& lhs, const var& rhs) { return var(lhs) < rhs; }
inline var operator<(double lhs, const var& rhs) { return var(lhs) < rhs; }
inline var operator<(long long lhs, const var& rhs) { return var(lhs) < rhs; }
inline var operator<(int lhs, const var& rhs) { return var(lhs) < rhs; }

inline var operator>(const char* lhs, const var& rhs) { return var(lhs) > rhs; }
inline var operator>(const std::string& lhs, const var& rhs) { return var(lhs) > rhs; }
inline var operator>(double lhs, const var& rhs) { return var(lhs) > rhs; }
inline var operator>(long long lhs, const var& rhs) { return var(lhs) > rhs; }
inline var operator>(int lhs, const var& rhs) { return var(lhs) > rhs; }

inline var operator<=(const char* lhs, const var& rhs) { return var(lhs) <= rhs; }
inline var operator<=(const std::string& lhs, const var& rhs) { return var(lhs) <= rhs; }
inline var operator<=(double lhs, const var& rhs) { return var(lhs) <= rhs; }
inline var operator<=(long long lhs, const var& rhs) { return var(lhs) <= rhs; }
inline var operator<=(int lhs, const var& rhs) { return var(lhs) <= rhs; }

inline var operator>=(const char* lhs, const var& rhs) { return var(lhs) >= rhs; }
inline var operator>=(const std::string& lhs, const var& rhs) { return var(lhs) >= rhs; }
inline var operator>=(double lhs, const var& rhs) { return var(lhs) >= rhs; }
inline var operator>=(long long lhs, const var& rhs) { return var(lhs) >= rhs; }
inline var operator>=(int lhs, const var& rhs) { return var(lhs) >= rhs; }

inline void print() {
    std::cout << std::endl;
}

template<typename T, typename... Args>
inline void print(T first, Args... args) {
    std::cout << first;
    if constexpr (sizeof...(args) > 0) {
        std::cout << " ";
        print(args...);
    } else {
        std::cout << std::endl;
    }
}

inline var input(const std::string& prompt = "") {
    if (!prompt.empty()) {
        std::cout << prompt;
    }
    std::string s;
    std::getline(std::cin, s);
    return var(s);
}

// Microcontroller abstractions for PC Simulation
enum PinMode {
    PIN_INPUT,
    PIN_OUTPUT
};

constexpr bool HIGH = true;
constexpr bool LOW = false;

class Pin {
private:
    int pin_num;
    PinMode mode;
    bool state = LOW;
    std::vector<std::function<void(var)>> listeners;

public:
    Pin() : pin_num(-1), mode(PIN_INPUT) {}
    Pin(int p, PinMode m) : pin_num(p), mode(m) {}

    void turnOn() {
        state = HIGH;
        trigger("change", state);
    }

    void turnOff() {
        state = LOW;
        trigger("change", state);
    }

    void write(const var& s) {
        state = s.toBool();
        trigger("change", var(state));
    }

    var read() const {
        return var(state);
    }

    void on(const std::string& event, std::function<void(var)> callback) {
        if (event == "change") {
            listeners.push_back(callback);
        }
    }

private:
    void trigger(const std::string& event, var value) {
        for (auto& cb : listeners) {
            cb(value);
        }
    }
};

using pin = Pin;

// Math Namespace
struct MathClass {
    var random() {
        return var((double)std::rand() / RAND_MAX);
    }
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

// JSON Namespace
struct JSONClass {
    std::string stringify(const var& v) {
        return v.toString();
    }
} JSON;

// File Namespace
struct FileClass {
    var read(const var& filename) {
        std::ifstream f(filename.toString());
        if (!f.is_open()) return var();
        std::stringstream ss;
        ss << f.rdbuf();
        return var(ss.str());
    }

    var write(const var& filename, const var& content) {
        std::ofstream f(filename.toString());
        if (!f.is_open()) return var(false);
        f << content.toString();
        return var(true);
    }

    var append(const var& filename, const var& content) {
        std::ofstream f(filename.toString(), std::ios_base::app);
        if (!f.is_open()) return var(false);
        f << content.toString();
        return var(true);
    }

    var exists(const var& filename) {
        std::ifstream f(filename.toString());
        return var(f.good());
    }

    var remove(const var& filename) {
        return var(std::remove(filename.toString().c_str()) == 0);
    }
} File;

// Object Namespace
struct ObjectClass {
    var keys(const var& obj) { return obj.keys(); }
    var values(const var& obj) { return obj.values(); }
    var entries(const var& obj) { return obj.entries(); }
} Object;

// Global helper functions
inline var isOdd(const var& v) { return v.isOdd(); }
inline var isEven(const var& v) { return v.isEven(); }

// Event Loop Engine
namespace DolphinRuntime {

struct CallbackTask {
    var callback;
    std::vector<var> args;
};

class EventLoop {
private:
    std::queue<CallbackTask> task_queue;
    std::mutex queue_mutex;
    std::atomic<int> pending_handles{0};

    EventLoop() = default;

public:
    static EventLoop& instance() {
        static EventLoop loop;
        return loop;
    }

    void ref() {
        pending_handles++;
    }

    void unref() {
        pending_handles--;
    }

    void queueCallback(const var& cb, const std::vector<var>& args = {}) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        task_queue.push({cb, args});
    }

    bool hasPendingTasks() {
        std::lock_guard<std::mutex> lock(queue_mutex);
        return !task_queue.empty() || pending_handles > 0;
    }

    void run() {
        while (true) {
            std::queue<CallbackTask> local_queue;
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (task_queue.empty() && pending_handles == 0) {
                    break;
                }
                std::swap(local_queue, task_queue);
            }

            while (!local_queue.empty()) {
                CallbackTask task = local_queue.front();
                local_queue.pop();
                if (task.callback.isFunction()) {
                    task.callback(task.args);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
};

inline void runEventLoop() {
    EventLoop::instance().run();
}

} // namespace DolphinRuntime

// Dolphin Namespace
struct DolphinClass {
    void async(const var& task, const var& callback) {
        DolphinRuntime::EventLoop::instance().ref();
        std::thread([task, callback]() {
            var result = var();
            if (task.isFunction()) {
                result = task(std::vector<var>{});
            }
            DolphinRuntime::EventLoop::instance().queueCallback(callback, {result});
            DolphinRuntime::EventLoop::instance().unref();
        }).detach();
    }
} Dolphin;

// Asynchronous timing functions
inline void setTimeout(const var& callback, const var& delay_ms) {
    long long delay = delay_ms.toInt();
    DolphinRuntime::EventLoop::instance().ref();
    std::thread([callback, delay]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        DolphinRuntime::EventLoop::instance().queueCallback(callback);
        DolphinRuntime::EventLoop::instance().unref();
    }).detach();
}

inline void setInterval(const var& callback, const var& interval_ms) {
    long long interval = interval_ms.toInt();
    DolphinRuntime::EventLoop::instance().ref();
    std::thread([callback, interval]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            DolphinRuntime::EventLoop::instance().queueCallback(callback);
        }
    }).detach();
}

inline void sleep(const var& ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms.toInt()));
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

// Global TCP and HTTP namespaces
struct TCPNamespace {
    var Server() {
        var s = var(var_object{});
        s.event_listeners = std::make_shared<std::map<std::string, std::vector<var>>>();
        s.tcp_server = std::make_shared<TCPServerClass>(s.event_listeners);
        return s;
    }
} TCP;

struct HTTPNamespace {
    var Server() {
        var s = var(var_object{});
        s.http_server = std::make_shared<HTTPServerClass>();
        return s;
    }
} HTTP;

