#include "dolphin_runtime.hpp"
using namespace DolphinRuntime;

var counter;

void dolphin_main() {
    std::srand(std::time(nullptr));
    print(var("--- Asynchronous Timers Test ---"));
    print(var("Main thread: starting timer tests"));
    setTimeout([=]() mutable -> var {
        print(var("[Timer] setTimeout fired after 1.5 seconds!"));
        return var();
    }, var(1500));
    counter = var(0);
    setInterval([=]() mutable -> var {
        counter = (counter + var(1));
        print((var("[Timer] setInterval interval tick: ") + counter));
        return var();
    }, var(1000));
    print(var("Main thread: sleeping for 4.5 seconds to let timers execute..."));
    sleep(var(4500));
    print(var("Main thread: waking up and exiting!"));
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
