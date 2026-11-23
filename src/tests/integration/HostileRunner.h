// -----------------------------------------------------------------------------
// HostileRunner -- runs pieces of work that must not be trusted.
//
// A piece of work that never finishes must not take the whole test with it.
// The work therefore runs on a thread of its own and the test watches from
// the outside. When the work does not finish in time the test says so and
// goes on.
//
// The thread that is stuck cannot be stopped: a thread that is inside a loop
// that never ends only stops when the process ends. The runner therefore
// leaves it alone when it is destroyed.
//
// The counters live in the runner and not on the stack of the test, so the
// test never reads a value that belongs to a thread that is gone. That was
// the way the first version of the fuzz tests died: the flag that said "the
// work is done" was on the stack of the caller and was read after it was
// gone.
// -----------------------------------------------------------------------------
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace vrutti::tests {

class HostileRunner {
public:
    HostileRunner() : m_done(0), m_started(0), m_stop(false) {
        m_thread = std::thread([this]() { this->run(); });
    }

    ~HostileRunner() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_signal.notify_all();
        if (m_thread.joinable()) {
            // The thread may be inside a piece of work that never finishes.
            // It cannot be joined without waiting for it, so it is left to
            // end with the process.
            m_thread.detach();
        }
    }

    HostileRunner(const HostileRunner&) = delete;
    HostileRunner& operator=(const HostileRunner&) = delete;

    // Hands a piece of work to the runner.
    //
    // The work is called without an argument; everything it needs is
    // captured by the caller. The work must not outlive what it captures.
    void push(std::function<void()> work) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_jobs.push_back(std::move(work));
        }
        m_signal.notify_one();
    }

    // Waits until every piece of work that was handed over is done.
    //
    // Returns false when the time is up. `started()` and `done()` then say
    // which piece of work is the one that hangs: the one after `done()`.
    bool waitIdle(int timeoutMs) {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(timeoutMs);
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_jobs.empty() || m_working) {
            if (m_signal.wait_until(lock, deadline) == std::cv_status::timeout) {
                return false;
            }
        }
        return true;
    }

    // How many pieces of work were done.
    size_t done() const { return m_done.load(); }

    // How many pieces of work were started. The one that hangs is the one
    // with the number `started() - 1`.
    size_t started() const { return m_started.load(); }

private:
    void run() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_signal.wait(lock, [this]() { return m_stop || !m_jobs.empty(); });
                if (m_jobs.empty()) {
                    if (m_stop) return;
                    continue;
                }
                job = std::move(m_jobs.front());
                m_jobs.pop_front();
                m_working = true;
                m_started.fetch_add(1);
            }

            job();

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_working = false;
                m_done.fetch_add(1);
            }
            m_signal.notify_all();
        }
    }

    mutable std::mutex m_mutex;
    std::condition_variable m_signal;
    std::deque<std::function<void()>> m_jobs;
    bool m_working = false;
    std::atomic<size_t> m_done;
    std::atomic<size_t> m_started;
    bool m_stop = false;
    std::thread m_thread;
};

} // namespace vrutti::tests
