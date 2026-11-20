# concurrency (src/core/concurrency/)

## Purpose
Concurrency helpers: the thread pool and the async primitives used by the core.

## Files
- **Async.h**: Header for the Async component.
- **CancellationToken.h**: Header for the CancellationToken component.
- **ThreadPool.cpp**: Implementation of the ThreadPool component.
- **ThreadPool.h**: Header for the ThreadPool component.
- **TODO.md**: Open questions and planned work for this folder.

## Public Methods
- **Delayer::~Delayer()**
- **Delayer::trigger(std::function<void()>, int)**
- **std::lock_guard<std::mutex> Delayer::lock(m_mutex)**
- **cancel()**
- **Throttler::queue(std::function<void()>)**
- **Throttler::runNext()**
- **runNext()**
- **std::lock_guard<std::mutex> lock(m_mutex)**
- **CancellationToken::~CancellationToken()**
- **bool CancellationToken::isCancellationRequested()**
- **vrutti::core::events::Event<void>& CancellationToken::onCancellationRequested()**
- **bool NoneCancellationToken::isCancellationRequested()**
- **vrutti::core::events::Event<void>& NoneCancellationToken::onCancellationRequested()**
- **CancellationTokenSource::cancel()**
- **m_token-> fire()**
- **std::shared_ptr<CancellationToken> token()**
- **bool MutableToken::isCancellationRequested()**
- **vrutti::core::events::Event<void>& MutableToken::onCancellationRequested()**
- **MutableToken::fire()**
- **ThreadPool::ThreadPool(size_t)**
- **ThreadPool::~ThreadPool()**
- **ThreadPool::ThreadPool(const ThreadPool&)**
- **std::unique_lock<std::mutex> lock(m_queueMutex)**
- **m_condition. notify_one()**
