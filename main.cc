#include <coroutine>
#include <cstdio>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <set>
#include <thread>

class Scheduler {
    std::set<std::pair<long long, std::coroutine_handle<>>> timeouts;
    std::queue<std::coroutine_handle<>> q;

  public:
    void run_all_ready() {
        while (ready_size()) {
            auto co = dequeue();
            co.resume();
        }
    }

    void wait_to_ready() {

        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch());
        long long curr = duration.count();

        for (auto it = timeouts.begin(); it != timeouts.end();) {
            auto [timeout, coro] = *it;
            if (timeout <= curr) {
                push_ready(coro);
                it = timeouts.erase(it);
            } else {
                break;
            }
        }

        /* sleep for 1ms */
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    static Scheduler& instance() {
        static Scheduler s;
        return s;
    }

    Scheduler() {}

    ~Scheduler() {}

    /* enqueue */
    void push_ready(std::coroutine_handle<> co) noexcept { q.push(co); }

    /* dequeue */
    std::coroutine_handle<> dequeue() noexcept {
        auto co = q.front();
        q.pop();
        return co;
    }

    int ready_size() noexcept { return q.size(); }

    int wait_size() noexcept { return timeouts.size(); }

    /* enqueue */
    void push_wait(long long timeout, std::coroutine_handle<> co) noexcept {
        timeouts.insert({timeout, co});
    }
};

template <typename T = void> struct task {
    struct promise_type {
        task get_return_object() noexcept {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() const noexcept { return {}; }
        std::suspend_never final_suspend() const noexcept { return {}; }
        void unhandled_exception() {}
    };

    std::coroutine_handle<promise_type> handle;
};

auto timeout(int ms) noexcept {
    struct awaiter {
        long long delay = 0;
        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) noexcept {
            auto& s = Scheduler::instance();

            auto now = std::chrono::system_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch());
            long long timeout = duration.count() + delay;

            s.push_wait(timeout, h);
        }

        void await_resume() const noexcept {}
    };

    return awaiter{ms};
}

task<> future_async() {

    /* suspend */
    co_await timeout(1);

    co_await timeout(1);

    co_await timeout(1);
}

// #define COROUTINE

int main() {

    constexpr int NUM_THREADS = 100 * 1000;

#if defined(COROUTINE)
    auto& s = Scheduler::instance();

    std::vector<task<>> tasks;
    for (auto i = 0; i < NUM_THREADS; ++i)
        s.push_ready(future_async().handle);

    int cnt = 0;

    while (s.ready_size() || s.wait_size()) {
        do {

            /* process all ready tasks */
            s.run_all_ready();

            /* move all applicable tasks from wait to ready */
            s.wait_to_ready();

        } while (s.ready_size());

        /* wait a bit */
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ms);

        ++cnt;
    }
    std::cout << cnt << std::endl;
#else

    std::vector<std::future<void>> futures;

    for (int i = 0; i < NUM_THREADS; ++i) {
        futures.emplace_back(std::async(std::launch::async, []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }));
    }

    // Wait for all futures to complete
    for (auto& f : futures) {
        f.get(); // Blocks until the async task is finished
    }

#endif

    return 0;
}