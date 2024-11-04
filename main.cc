#include <coroutine>
#include <cstdio>
#include <queue>
#include <thread>

class CTP {
    std::queue<std::coroutine_handle<>> q;

  public:
    // For simplicity, we'll assume a global singleton instance is available
    static CTP& instance();

    // The constructor should spawn thread_count threads and begin the run loop
    // of each thread
    CTP(int thread_count);

    // When a coroutine is enqueued, the coroutine is passed to one of the CTP
    // threads, and eventually coroutine.resume() should be invoked when the
    // thread is ready. You can implement work stealing here to "steal"
    // coroutines on busy threads from idle ones, or any other load
    // balancing/scheduling scheme of your choice.
    //
    // NOTE: This should (obviously) be threadsafe
    void enqueue(std::coroutine_handle<> co) { q.push(co); }
};

struct event_awaiter {
    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> coroutine) const noexcept {
        std::thread thread{[coroutine]() noexcept {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(2000ms);
            CTP::instance().enqueue(coroutine);
        }};
        thread.detach();
    }

    void await_resume() noexcept {
        // This is called after the coroutine is resumed in the async thread
        // printf("Event signaled, resuming on thread %i\n", get_thread_id());
    }
};

int main() {}