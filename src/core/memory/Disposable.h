#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>

namespace vrutti::core::memory {

    // Ported from VS Code's `vs/base/common/lifecycle.ts`
    // Represents an object that holds unmanaged resources or event subscriptions
    // that must be explicitly released.
    class IDisposable {
    public:
        virtual ~IDisposable() = default;
        virtual void dispose() = 0;
    };

    // A collection of disposables that are disposed together.
    // Equivalent to VS Code's `DisposableStore`.
    class DisposableStore : public IDisposable {
    public:
        DisposableStore() : m_isDisposed(false) {}

        ~DisposableStore() override {
            dispose();
        }

        // Add a disposable to the store. 
        // If the store is already disposed, the item is disposed immediately.
        void add(std::unique_ptr<IDisposable> disposable) {
            if (!disposable) return;

            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_isDisposed) {
                disposable->dispose();
                return;
            }
            m_disposables.push_back(std::move(disposable));
        }

        void dispose() override {
            std::vector<std::unique_ptr<IDisposable>> toDispose;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_isDisposed) return;
                m_isDisposed = true;
                toDispose = std::move(m_disposables);
            }

            // Dispose in reverse order of addition
            for (auto it = toDispose.rbegin(); it != toDispose.rend(); ++it) {
                if (*it) {
                    (*it)->dispose();
                }
            }
        }

    private:
        std::vector<std::unique_ptr<IDisposable>> m_disposables;
        std::mutex m_mutex;
        bool m_isDisposed;
    };

} // namespace vrutti::core::memory
