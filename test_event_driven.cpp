#include "dolphin_runtime.hpp"
using namespace DolphinRuntime;

var emitter;

int main() {
    std::srand(std::time(nullptr));
    print(var("--- Dolphin Event-Driven Architecture Test ---"));
    print(var("\n1. Testing Universal Event Emitter:"));
    emitter = var_object{};
    emitter.on(var("data"), [=](var payload) mutable {
            print((var("  [Event .on] Received data payload: ") + payload));
        }
    );
    emitter.once(var("only_once"), [=](var msg) mutable {
            print((var("  [Event .once] Fired with: ") + msg));
        }
    );
    print(var("  Emitting 'data' event first time:"));
    emitter.emit(var("data"), var("Hello Dolphin!"));
    print(var("  Emitting 'only_once' event first time:"));
    emitter.emit(var("only_once"), var("This runs once"));
    print(var("  Emitting 'only_once' event second time:"));
    emitter.emit(var("only_once"), var("This should NOT run"));
    emitter.off(var("data"));
    print(var("  Emitting 'data' event after .off():"));
    emitter.emit(var("data"), var("No one should hear this"));
    print(var("\n2. Testing Asynchronous Background Worker:"));
    print(var("  [Main Thread] Spawing parallel CPU-intense computation..."));
    Dolphin.async([=]() mutable {
            var sum = var(0);
            for (var i = var(1); i < var(1000000); ++i) {
                sum += i;
            }
            return sum;
        }
    , [=](var result) mutable {
            print((var("  [Main Thread Callback] Background computation complete! Sum = ") + result));
        }
    );
    print(var("  [Main Thread] Background worker spawned! Main thread continues immediately..."));
    print(var("\n3. Testing Timer (setTimeout) & Event Loop Keep-Alive:"));
    setTimeout([=]() mutable {
            print(var("  [Main Thread Timer] Timeout fired after 500ms!"));
        }
    , var(500));
    print(var("  [Main Thread] setTimeout scheduled! Script reached the end. Event loop should keep it alive..."));
    DolphinRuntime::runEventLoop();
    return 0;
}
