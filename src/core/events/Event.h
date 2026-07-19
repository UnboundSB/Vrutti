#pragma once
#include <functional>
#include <vector>
#include <mutex>
#include <memory>
#include <algorithm>
#include "core/memory/Disposable.h"

namespace vrutti::core::events {

    template<typename T>
    using Listener = std::function<void(const T&)>;

    // An event that listeners can subscribe to.
    template<typename T>
    class Event {
    public:
        // Subscribes a listener to the event and returns an IDisposable.
        // Calling dispose() on the returned object will unsubscribe the listener.
        virtual std::unique_ptr<memory::IDisposable> operator()(Listener<T> listener) = 0;
        virtual ~Event() = default;
    };

    // Emitter that triggers the event.
    template<typename T>
    class Emitter : public Event<T> {
    private:
        struct ListenerEntry {
            uint64_t id;
            Listener<T> callback;
        };

        class Subscription : public memory::IDisposable {
        public:
            Subscription(Emitter* emitter, uint64_t id)
                : m_emitter(emitter), m_id(id) {}

            void dispose() override {
                if (m_emitter) {
                    m_emitter->removeListener(m_id);
                    m_emitter = nullptr;
                }
            }
        private:
            Emitter* m_emitter;
            uint64_t m_id;
        };

    public:
        Emitter() : m_nextId(1) {}
        
        ~Emitter() override {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_listeners.clear();
        }

        std::unique_ptr<memory::IDisposable> operator()(Listener<T> listener) override {
            std::lock_guard<std::mutex> lock(m_mutex);
            uint64_t id = m_nextId++;
            m_listeners.push_back({id, std::move(listener)});
            return std::make_unique<Subscription>(this, id);
        }

        void fire(const T& eventData) {
            // Copy listeners to avoid deadlocks if a listener modifies the list
            std::vector<ListenerEntry> listenersCopy;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                listenersCopy = m_listeners;
            }

            for (const auto& entry : listenersCopy) {
                if (entry.callback) {
                    entry.callback(eventData);
                }
            }
        }

    private:
        void removeListener(uint64_t id) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_listeners.erase(
                std::remove_if(m_listeners.begin(), m_listeners.end(),
                               [id](const ListenerEntry& entry) { return entry.id == id; }),
                m_listeners.end());
        }

        std::vector<ListenerEntry> m_listeners;
        std::mutex m_mutex;
        uint64_t m_nextId;
    };

} // namespace vrutti::core::events
