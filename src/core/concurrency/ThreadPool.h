#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

namespace vrutti::core::concurrency {

    // A lightweight ThreadPool for executing asynchronous background tasks
    // designed to keep the main IDE UI thread highly responsive.
    class ThreadPool {
    public:
        // Initialize the thread pool with a specific number of hardware threads.
        // If 0, it defaults to the system's hardware concurrency limit.
        explicit ThreadPool(size_t threads = 0);
        ~ThreadPool();

        // Delete copy/move semantics
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        // Enqueue a new task into the thread pool.
        // Returns a std::future containing the result of the task.
        template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args) 
            -> std::future<typename std::invoke_result<F, Args...>::type>;

    private:
        // The worker thread loop
        void workerLoop();

        std::vector<std::thread> m_workers;
        std::queue<std::function<void()>> m_tasks;

        std::mutex m_queueMutex;
        std::condition_variable m_condition;
        bool m_stop;
    };

    // Template implementation must be in the header
    template<class F, class... Args>
    auto ThreadPool::enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> 
    {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
            
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            if(m_stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            m_tasks.emplace([task](){ (*task)(); });
        }
        m_condition.notify_one();
        return res;
    }

} // namespace vrutti::core::concurrency
