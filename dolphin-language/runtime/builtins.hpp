#pragma once
#include "core.hpp"

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
    Pin(const var& p, PinMode m)
        : pin_num((int)p.toInt()), mode(m) {}

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

    void on(const var& event, const var& callback) {
        if (event.toString() == "change" && callback.isFunction()) {
            listeners.push_back([callback](var value) mutable {
                callback(std::vector<var>{value});
            });
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

struct JSONClass {
    std::string stringify(const var& v) {
        return v.toString();
    }
} JSON;

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

struct ObjectClass {
    var keys(const var& obj) { return obj.keys(); }
    var values(const var& obj) { return obj.values(); }
    var entries(const var& obj) { return obj.entries(); }
} Object;

inline var isOdd(const var& v) { return v.isOdd(); }
inline var isEven(const var& v) { return v.isEven(); }

inline void dolphin_init() {
    std::srand(std::time(nullptr));
}

template<typename... Args>
inline void dolphin_print(Args... args) {
    print(args...);
}

template<typename... Args>
inline void dolphin_println(Args... args) {
    print(args...);
}

inline var dolphin_input(const std::string& prompt = "") {
    return input(prompt);
}

inline var dolphin_len(const var& v) {
    return v.size();
}

inline var dolphin_range(const var& start, const var& end = var(), const var& step = var(1)) {
    var_array arr;
    long long s = 0;
    long long e = 0;
    long long st = step.toInt();
    if (end.getType() == var::TYPE_NULL) {
        e = start.toInt();
    } else {
        s = start.toInt();
        e = end.toInt();
    }
    if (st > 0) {
        for (long long i = s; i < e; i += st) {
            arr.push_back(var(i));
        }
    } else if (st < 0) {
        for (long long i = s; i > e; i += st) {
            arr.push_back(var(i));
        }
    }
    return var(arr);
}

inline var dolphin_typeof(const var& v) {
    switch (v.getType()) {
        case var::TYPE_NULL: return var("null");
        case var::TYPE_BOOL: return var("boolean");
        case var::TYPE_INT: return var("number");
        case var::TYPE_DOUBLE: return var("number");
        case var::TYPE_STRING: return var("string");
        case var::TYPE_ARRAY: return var("array");
        case var::TYPE_OBJECT: return var("object");
        case var::TYPE_FUNCTION: return var("function");
        case var::TYPE_PROMISE: return var("promise");
        case var::TYPE_MATRIX: return var("matrix");
        default: return var("undefined");
    }
}

inline var dolphin_parseInt(const var& v) {
    return var(v.toInt());
}

inline var dolphin_parseFloat(const var& v) {
    return var(v.toDouble());
}

inline var dolphin_str(const var& v) {
    return var(v.toString());
}

inline var dolphin_int(const var& v) {
    return var(v.toInt());
}

inline var dolphin_float(const var& v) {
    return var(v.toDouble());
}

inline var dolphin_pow(const var& base, const var& exp) {
    return Math.pow(base, exp);
}

inline var dolphin_bitnot(const var& v) {
    return var(~v.toInt());
}
