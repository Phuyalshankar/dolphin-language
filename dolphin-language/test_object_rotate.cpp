#include "dolphin_runtime.hpp"


int main() {
    std::srand(std::time(nullptr));
    print("--- Array of Objects Shifting Test ---");

    // 1. Array containing complex objects
    var leds = var_array{
    var_object{ {"id", 1}, {"color", "red"}, {"status", "ON" }},
    var_object{ {"id", 2}, {"color", "green"}, {"status", "OFF" }},
    var_object{ {"id", 3}, {"color", "blue"}, {"status", "OFF" }}
    };

    print("Initial sequence:");
    for (auto led : leds) {
    print("  LED " + led["id"] + " (" + led["color"] + "): " + led["status"]);
    }

    // 2. Rotate the array of objects right by 1 step
    print("\nAfter shifting right by 1:");
    var leds = leds.rotateRight(1);

    for (auto led : leds) {
    print("  LED " + led["id"] + " (" + led["color"] + "): " + led["status"]);
    }
    return 0;
}
