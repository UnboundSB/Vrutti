# events (src/core/events/)

## Purpose
Event bus: the in-process publish and subscribe mechanism.

## Files
- **Event.h**: Header for the Event component.
- **Observable.h**: Header for the Observable component.
- **TODO.md**: Open questions and planned work for this folder.

## Public Methods
- **Event::~Event()**
- **Subscription::dispose()**
- **m_emitter-> Subscription::removeListener(m_id)**
- **~Emitter()**
- **fire(const T&)**
- **std::lock_guard<std::mutex> lock(m_mutex)**
- **removeListener(uint64_t)**
- **IObserver::~IObserver()**
- **IObserver::beginUpdate()**
- **IObserver::endUpdate()**
- **IObserver::handleChange()**
- **const T& Observable::get()**
- **Observable::set(T)**
- **std::lock_guard<std::mutex> Observable::lock(m_mutex)**
- **obs-> beginUpdate()**
- **obs-> handleChange()**
- **obs-> endUpdate()**
- **addObserver(IObserver*)**
- **removeObserver(IObserver*)**
- **Autorun::~Autorun()**
- **Autorun::dispose()**
- **beginUpdate()**
- **endUpdate()**
- **handleChange()**
