#include "dolphin_runtime.hpp"
using namespace DolphinRuntime;

var name;
var version;
var x;
var y;
var z;
var numbers;
var greet(var user) {
    return ((var("Hello, ") + user) + var("!"));
}

var message;

void dolphin_main() {
    std::srand(std::time(nullptr));
    name = var("Dolphin");
    version = var(1.0);
    print((((var("Welcome to ") + name) + var(" version ")) + version));
    if ((version >= var(1.0))) {
            print(var("Version is ready!"));
        } else {
            print(var("Under development."));
        }
    x = var(10);
    y = var(20);
    z = (x + (y * var(2)));
    print((var("z is: ") + z));
    numbers = var_array{var(10), var(20), var(30)};
    numbers.add(var(40));
    print((var("First number: ") + numbers[var(0)]));
    print((var("Added number: ") + numbers[var(3)]));
    print((var("Full list: ") + numbers));
    message = greet(var("Developer"));
    print(message);
    print(var("Starting Pin simulation..."));
    pin led = Pin(var(13), PIN_OUTPUT);
    pin button = Pin(var(2), PIN_INPUT);
    button.on(var("change"), [=](var state) mutable -> var {
        if ((state == HIGH)) {
            print(var("Button pressed -> turning LED ON"));
            led.turnOn();
        } else {
            print(var("Button released -> turning LED OFF"));
            led.turnOff();
        }
        return var();
    });
    led.on(var("change"), [=](var state) mutable -> var {
        print((var("LED state changed to: ") + state));
        return var();
    });
    print(var("Simulating button press:"));
    button.write(HIGH);
    print(var("Simulating button release:"));
    button.write(LOW);
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
