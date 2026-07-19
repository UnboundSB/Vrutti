#pragma once
#include <atomic>
#include <memory>
#include "core/events/Event.h"

namespace vrutti::core::concurrency {

    // A token passed to async/background operations to allow them to be cooperatively cancelled.
    class CancellationToken {
    public:
        virtual ~CancellationToken() = default;
        
        // Returns true if cancellation has been requested
        virtual bool isCancellationRequested() const = 0;
        
        // Fired exactly once when cancellation is requested
        virtual vrutti::core::events::Event<void>& onCancellationRequested() = 0;
    };

    // A token that can never be cancelled
    class NoneCancellationToken : public CancellationToken {
    public:
        bool isCancellationRequested() const override { return false; }
        vrutti::core::events::Event<void>& onCancellationRequested() override {
            return m_event; // Never fires
        }
    private:
        vrutti::core::events::Event<void> m_event;
    };

    // The source of a cancellation token. This is held by the caller who wants to cancel the operation.
    class CancellationTokenSource {
    public:
        CancellationTokenSource() 
            : m_isCancelled(false), m_token(std::make_shared<MutableToken>(m_isCancelled)) {}

        // Trigger the cancellation
        void cancel() {
            if (m_isCancelled.exchange(true)) {
                return; // Already cancelled
            }
            m_token->fire();
        }

        // Retrieve the token to pass to the running task
        std::shared_ptr<CancellationToken> token() const {
            return m_token;
        }

    private:
        class MutableToken : public CancellationToken {
        public:
            explicit MutableToken(std::atomic<bool>& isCancelledRef) : m_isCancelledRef(isCancelledRef) {}
            
            bool isCancellationRequested() const override {
                return m_isCancelledRef.load();
            }

            vrutti::core::events::Event<void>& onCancellationRequested() override {
                return m_event;
            }

            void fire() {
                m_emitter.fire();
            }

        private:
            std::atomic<bool>& m_isCancelledRef;
            vrutti::core::events::Emitter<void> m_emitter;
            vrutti::core::events::Event<void> m_event{m_emitter.event()};
        };

        std::atomic<bool> m_isCancelled;
        std::shared_ptr<MutableToken> m_token;
    };

} // namespace vrutti::core::concurrency
