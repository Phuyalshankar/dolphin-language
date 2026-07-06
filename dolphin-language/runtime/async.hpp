#pragma once
#include "core.hpp"

// Event Loop Engine
namespace DolphinRuntime {

struct CallbackTask {
    var callback;
    std::vector<var> args;
};

class EventLoop {
private:
    std::queue<CallbackTask> task_queue;
    std::mutex queue_mutex;
    std::atomic<int> pending_handles{0};

    EventLoop() = default;

public:
    static EventLoop& instance() {
        static EventLoop loop;
        return loop;
    }

    void ref() {
        pending_handles++;
    }

    void unref() {
        pending_handles--;
    }

    void queueCallback(const var& cb, const std::vector<var>& args = {}) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        task_queue.push({cb, args});
    }

    bool hasPendingTasks() {
        std::lock_guard<std::mutex> lock(queue_mutex);
        return !task_queue.empty() || pending_handles > 0;
    }

    void run() {
        while (true) {
            std::queue<CallbackTask> local_queue;
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (task_queue.empty() && pending_handles == 0) {
                    break;
                }
                std::swap(local_queue, task_queue);
            }

            while (!local_queue.empty()) {
                CallbackTask task = local_queue.front();
                local_queue.pop();
                if (task.callback.isFunction()) {
                    task.callback(task.args);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    void runOnce() {
        std::queue<CallbackTask> local_queue;
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!task_queue.empty()) {
                std::swap(local_queue, task_queue);
            }
        }
        while (!local_queue.empty()) {
            CallbackTask task = local_queue.front();
            local_queue.pop();
            if (task.callback.isFunction()) {
                task.callback(task.args);
            }
        }
    }
};

inline void runEventLoop() {
    EventLoop::instance().run();
}

inline void runEventLoopOnce() {
    EventLoop::instance().runOnce();
}

} // namespace DolphinRuntime

// Dolphin Namespace
struct DolphinClass {
    void async(const var& task, const var& callback = var()) {
        DolphinRuntime::EventLoop::instance().ref();
        std::thread([task, callback]() {
            var result = var();
            if (task.isFunction()) {
                result = task(std::vector<var>{});
            }
            if (callback.isFunction()) {
                DolphinRuntime::EventLoop::instance().queueCallback(callback, {result});
            }
            DolphinRuntime::EventLoop::instance().unref();
        }).detach();
    }
    void sleep(const var& ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms.toInt()));
    }
} Dolphin;

// Asynchronous timing functions
inline void setTimeout(const var& callback, const var& delay_ms) {
    long long delay = delay_ms.toInt();
    DolphinRuntime::EventLoop::instance().ref();
    std::thread([callback, delay]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        DolphinRuntime::EventLoop::instance().queueCallback(callback);
        DolphinRuntime::EventLoop::instance().unref();
    }).detach();
}

inline void setInterval(const var& callback, const var& interval_ms) {
    long long interval = interval_ms.toInt();
    DolphinRuntime::EventLoop::instance().ref();
    std::thread([callback, interval]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            DolphinRuntime::EventLoop::instance().queueCallback(callback);
        }
    }).detach();
}

inline void sleep(const var& ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms.toInt()));
}
