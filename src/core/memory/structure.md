# memory (src/core/memory/)

## Purpose
Memory helpers: allocators and the shared buffer utilities.

## Files
- **ArenaAllocator.cpp**: Implementation of the ArenaAllocator component.
- **ArenaAllocator.h**: Header for the ArenaAllocator component.
- **Disposable.h**: Header for the Disposable component.
- **TODO.md**: Open questions and planned work for this folder.

## Public Methods
- **ArenaAllocator::ArenaAllocator(size_t)**
- **ArenaAllocator::~ArenaAllocator()**
- **ArenaAllocator::ArenaAllocator(const ArenaAllocator&)**
- **void* ArenaAllocator::allocate(size_t, size_t)**
- **template <typename T, typename... Args> T* ArenaAllocator::allocateObject(Args&&...)**
- **ArenaAllocator::reset()**
- **IDisposable::~IDisposable()**
- **IDisposable::dispose()**
- **DisposableStore::~DisposableStore()**
- **DisposableStore::add(std::unique_ptr<IDisposable>)**
- **disposable-> DisposableStore::dispose()**
- **m_disposables. push_back(std::move(disposable))**
- **dispose()**
- **std::lock_guard<std::mutex> lock(m_mutex)**
