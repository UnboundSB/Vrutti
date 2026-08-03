#pragma once
#include <functional>
#include <vector>
#include <memory>
#include <mutex>
#include "core/memory/Disposable.h"

namespace vrutti::core::events {

    class IObserver {
    public:
        virtual ~IObserver() = default;
        virtual void beginUpdate() = 0;
        virtual void endUpdate() = 0;
        virtual void handleChange() = 0;
    };

    // A lightweight reactive wrapper for values.
    // When the value changes, it notifies registered observers.
    template<typename T>
    class Observable {
    public:
        explicit Observable(T initialValue) : m_value(std::move(initialValue)) {}

        const T& get() const {
            return m_value;
        }

        void set(T newValue) {
            std::vector<IObserver*> observersCopy;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_value == newValue) {
                    return; // Unchanged
                }
                m_value = std::move(newValue);
                observersCopy = m_observers;
            }
            
            for (auto* obs : observersCopy) {
                try {
                    obs->beginUpdate();
                    obs->handleChange();
                    obs->endUpdate();
                } catch (...) {
                    // Ignore observer exceptions
                }
            }
        }

        // Register an observer to be notified on changes
        void addObserver(IObserver* observer) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_observers.push_back(observer);
        }

        void removeObserver(IObserver* observer) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_observers.erase(std::remove(m_observers.begin(), m_observers.end(), observer), m_observers.end());
        }

    private:
        // notifyObservers removed, logic moved to set() to avoid deadlock

        T m_value;
        mutable std::mutex m_mutex;
        std::vector<IObserver*> m_observers;
    };

    // Helper to automatically run a function when observed values change.
    class Autorun : public IObserver, public vrutti::core::memory::IDisposable {
    public:
        template<typename T>
        Autorun(Observable<T>& observable, std::function<void()> effect) 
            : m_effect(std::move(effect)), m_remove([&observable, this](){ observable.removeObserver(this); }) 
        {
            observable.addObserver(this);
            m_effect(); // Run initially
        }

        ~Autorun() override {
            dispose();
        }

        void dispose() override {
            if (m_remove) {
                m_remove();
                m_remove = nullptr;
            }
        }

        void beginUpdate() override {}
        void endUpdate() override {}
        void handleChange() override {
            m_effect();
        }

    private:
        std::function<void()> m_effect;
        std::function<void()> m_remove;
    };

} // namespace vrutti::core::events
