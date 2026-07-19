#include "ThreadPool.h"

namespace vrutti::core::concurrency {

    ThreadPool::ThreadPool(size_t threads)
        : m_stop(false)
    {
        if (threads == 0) {
            threads = std::thread::hardware_concurrency();
            if (threads == 0) threads = 4; // Fallback
        }

        for(size_t i = 0; i < threads; ++i) {
            m_workers.emplace_back([this] { this->workerLoop(); });
        }
    }

    ThreadPool::~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_stop = true;
        }
        m_condition.notify_all();
        for(std::thread &worker: m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void ThreadPool::workerLoop() {
        while(true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(this->m_queueMutex);
                this->m_condition.wait(lock, [this]{ 
                    return this->m_stop || !this->m_tasks.empty(); 
                });
                
                if(this->m_stop && this->m_tasks.empty()) {
                    return;
                }
                
                task = std::move(this->m_tasks.front());
                this->m_tasks.pop();
            }
            task();
        }
    }

} // namespace vrutti::core::concurrency
