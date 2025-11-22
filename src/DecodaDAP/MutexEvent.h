#include <condition_variable>
#include <mutex>

struct MutexEvent {
    std::mutex mtx;
    std::condition_variable cv;
    bool signaled = false;

    // Signal the event (wake up one waiting thread)
    void fire() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            signaled = true;
        }
        cv.notify_one();
    }

    // Wait for the event to be signaled
    void wait() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return signaled; });
    }

    // Optionally, reset the event for reuse
    void reset() {
        std::lock_guard<std::mutex> lock(mtx);
        signaled = false;
    }
};