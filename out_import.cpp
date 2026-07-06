#include "dolphin_runtime.hpp"
using namespace DolphinRuntime;

var add(var a, var b) {
    return (a + b);
}

var multiply(var a, var b) {
    return (a * b);
}

var sum;
var product;

void dolphin_main() {
    std::srand(std::time(nullptr));
    print(var("--- Import System Test ---"));
    sum = add(var(10), var(20));
    print((var("Adding 10 + 20: ") + sum));
    product = multiply(var(5), var(8));
    print((var("Multiplying 5 * 8: ") + product));
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
