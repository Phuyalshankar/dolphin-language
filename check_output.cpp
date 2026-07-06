#include "dolphin_runtime.hpp"

var add(var a, var b) {
return a + b;
}
var multiply(var a, var b) {
return a * b;
}

int main() {
    std::srand(std::time(nullptr));
    print("--- Import System Test ---");

    // Import the helper module
    // Helper functions for import test


    // Use functions from the helper module
    var sum = add(10, 20);
    print("Adding 10 + 20: " + sum);

    var product = multiply(5, 8);
    print("Multiplying 5 * 8: " + product);
    return 0;
}
