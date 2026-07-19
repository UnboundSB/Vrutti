#pragma once
#include <string>
#include <vector>
#include <memory>
#include "core/events/Event.h"
#include "core/concurrency/CancellationToken.h"

namespace vrutti::core::fs {

    // Represents a chunk of data emitted by a stream
    struct StreamChunk {
        const uint8_t* data;
        size_t size;
        bool isString;
    };

    // A generic readable stream interface for chunked data loading
    class IReadableStream {
    public:
        virtual ~IReadableStream() = default;

        // Fired when a new chunk of data is available
        virtual vrutti::core::events::Event<StreamChunk>& onData() = 0;
        
        // Fired when the stream throws an error
        virtual vrutti::core::events::Event<std::string>& onError() = 0;
        
        // Fired when the stream has reached the end
        virtual vrutti::core::events::Event<void>& onEnd() = 0;

        // Pauses the stream from emitting onData events
        virtual void pause() = 0;
        
        // Resumes the stream
        virtual void resume() = 0;

        // Destroys the stream and stops all operations
        virtual void destroy() = 0;
    };

    // A helper to buffer incoming stream chunks into memory.
    // Useful when bridging from streams into systems that require contiguous memory (like strings).
    class StreamBuffer {
    public:
        void append(const StreamChunk& chunk) {
            size_t currentSize = m_buffer.size();
            m_buffer.resize(currentSize + chunk.size);
            std::memcpy(m_buffer.data() + currentSize, chunk.data, chunk.size);
        }

        std::string toString() const {
            return std::string(reinterpret_cast<const char*>(m_buffer.data()), m_buffer.size());
        }

        void clear() {
            m_buffer.clear();
        }

    private:
        std::vector<uint8_t> m_buffer;
    };

} // namespace vrutti::core::fs
