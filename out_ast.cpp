#include "dolphin_runtime.hpp"
using namespace DolphinRuntime;

var val;
var user;
var sum;
var items;
var outerFunc(var x) {
    var innerFunc = [=](var y) mutable -> var {
    return (x + y);
};
    return innerFunc(var(10));
}


void dolphin_main() {
    std::srand(std::time(nullptr));
    val = ((var(2) + (var(3) * Math.pow(var(4), var(2)))) << var(1));
    print((var("Precedence test: 2 + 3 * 4 ** 2 << 1 = ") + val));
    if ((val == var(100))) {
            print(var("  [SUCCESS] Operator precedence is correct!"));
        } else {
            print((var("  [FAILED] Operator precedence mismatch: ") + val));
        }
    user = var_object{{"name", var("Aashish")}, {"roles", var_array{var("developer"), var("admin")}}, {"details", var_object{{"status", var("active")}, {"score", var(98.5)}}}};
    print(var("Object test:"));
    print((var("  Name: ") + user[var("name")]));
    print((var("  First Role: ") + user[var("roles")][var(0)]));
    print((var("  Status: ") + user[var("details")][var("status")]));
    print(var("Loop tests:"));
    sum = var(0);
    for (var i = var(1); i < var(6); ++i) {
            sum = (sum + i);
        }
    print((var("  Range loop sum (1..5): ") + sum));
    items = var_array{var("apple"), var("banana"), var("cherry")};
    for (var item : items) {
            print((var("  Foreach item: ") + item));
        }
    print((var("Nested function test: outerFunc(5) = ") + outerFunc(var(5))));
}

#ifdef ARDUINO
void setup() {
    dolphin_main();
}
void loop() {
    DolphinRuntime::runEventLoopOnce();
}
#else
int main() {
    dolphin_main();
    DolphinRuntime::runEventLoop();
    return 0;
}
#endif
