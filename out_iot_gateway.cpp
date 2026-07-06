#include "dolphin_runtime.hpp"
using namespace DolphinRuntime;

var relayPin;
var sensorPin;
var bt;

void dolphin_main() {
    std::srand(std::time(nullptr));
    print(var("=== Starting Dolphin Smart Home IoT Gateway ==="));
    relayPin = var(2);
    sensorPin = var(34);
    GPIO.mode(relayPin, GPIO.OUTPUT);
    GPIO.mode(sensorPin, GPIO.INPUT);
    WiFi.connect(var("Home_Network"), var("WiFiSecurePassword"));
    bt = Bluetooth.Serial();
    bt.on(var("connect"), [=](var device) mutable -> var {
        print((var("[Bluetooth] Remote device connected: ") + device));
        return var();
    });
    bt.on(var("data"), [=](var command) mutable -> var {
        var cmd = command.trim();
        print(((var("[Bluetooth Command] Received: '") + cmd) + var("'")));
        if ((cmd == var("relay:on"))) {
            GPIO.write(relayPin, GPIO.HIGH);
            bt[var("write")](var("SUCCESS: Relay Pin 2 set to HIGH\n"));
        } else if ((cmd == var("relay:off"))) {
            GPIO.write(relayPin, GPIO.LOW);
            bt[var("write")](var("SUCCESS: Relay Pin 2 set to LOW\n"));
        } else {
            bt[var("write")](((var("ERROR: Unknown command '") + cmd) + var("'\n")));
        }
        return var();
    });
    bt[var("start")](var("Smart_Home_Gateway"));
    Dolphin.async([=]() mutable -> var {
        while (var(true)) {
            Dolphin.sleep(var(2000));
            var sensorVal = GPIO.analogRead(sensorPin);
            print((((var("[Telemetry] Sensor Reading (Pin ") + sensorPin) + var("): ")) + sensorVal));
            if ((WiFi.status() == var("connected"))) {
                print(((var("[Telemetry] Local Gateway IP: ") + WiFi.ip()) + var(" (WiFi Status: connected)")));
            } else {
                print(var("[Telemetry] WiFi Disconnected!"));
            }
        }
        return var();
    });
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
