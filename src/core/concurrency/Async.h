#pragma once
#include <functional>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <atomic>

namespace vrutti::core::concurrency {

    // A helper to delay the execution of a task by a given number of milliseconds.
    // If triggered again before the delay expires, the delay resets.
    class Delayer {
    public:
        explicit Delayer(int defaultDelayMs) : m_defaultDelay(defaultDelayMs), m_cancelFlag(std::make_shared<std::atomic<bool>>(false)) {}

        ~Delayer() {
            cancel();
        }

        // Triggers the task. Overwrites any previously scheduled execution.
        void trigger(std::function<void()> task, int delayMs = -1) {
            cancel(); // Cancel any existing scheduled task
            
            auto currentCancelFlag = std::make_shared<std::atomic<bool>>(false);
            
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_cancelFlag = currentCancelFlag;
            }

            int waitTime = delayMs >= 0 ? delayMs : m_defaultDelay;

            // Fire and forget a thread to handle the delay.
            // In a production event loop, this would hook into the OS timer queue instead.
            std::thread([task, waitTime, currentCancelFlag]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
                if (!currentCancelFlag->load()) {
                    task();
                }
            }).detach();
        }

        void cancel() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cancelFlag->store(true);
        }

    private:
        int m_defaultDelay;
        std::mutex m_mutex;
        std::shared_ptr<std::atomic<bool>> m_cancelFlag;
    };

    // A throttler ensures that only one task is executed at a time.
    // If multiple tasks are triggered while one is running, only the last one is queued to run next.
    class Throttler {
    public:
        void queue(std::function<void()> task) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_nextTask = task;
            
            if (!m_isRunning) {
                m_isRunning = true;
                runNext();
            }
        }

    private:
        void runNext() {
            std::function<void()> taskToRun;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_nextTask) {
                    m_isRunning = false;
                    return;
                }
                taskToRun = m_nextTask;
                m_nextTask = nullptr;
            }

            // Execute in a background thread to unblock the caller
            std::thread([this, taskToRun]() {
                taskToRun();
                this->runNext();
            }).detach();
        }

        std::mutex m_mutex;
        bool m_isRunning = false;
        std::function<void()> m_nextTask = nullptr;
    };

} // namespace vrutti::core::concurrency
