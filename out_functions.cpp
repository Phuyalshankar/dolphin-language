#include "dolphin_runtime.hpp"
using namespace DolphinRuntime;

var add(var a, var b) {
    return (a + b);
}

var subtract(var a, var b) {
    return (a - b);
}

var operation;
var result;
var execute(var x, var y, var cb) {
    return cb(x, y);
}

var multiply;

void dolphin_main() {
    std::srand(std::time(nullptr));
    print(var("--- First-Class Functions Test ---"));
    operation = add;
    print((var("Operation representation: ") + operation));
    result = operation(var(15), var(25));
    print((var("Result of calling operation(15, 25): ") + result));
    operation = subtract;
    print((var("Reassigned operation(50, 20): ") + operation(var(50), var(20))));
    print((var("Passing add callback: ") + execute(var(10), var(5), add)));
    print((var("Passing subtract callback: ") + execute(var(10), var(5), subtract)));
    multiply = [=](var a, var b) mutable -> var {
    {
            return (a * b);
        }
    ;
    print((var("Lambda multiply(6, 7): ") + multiply(var(6), var(7))));
    print((var("Passing lambda callback: ") + execute(var(4), var(5), multiply)));
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
