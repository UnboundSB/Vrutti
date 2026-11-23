// -----------------------------------------------------------------------------
// IpcFuzzTest -- the pipe under hostile input.
//
// The extension host is a separate process. It is usually the real thing,
// but a broken build, a wrong version or a program that found the pipe can
// send anything at all.
//
// The core therefore has to survive:
//
//   * frames that are not JSON,
//   * frames that are JSON but mean something else,
//   * frames that are cut in half,
//   * a frame that never ends,
//   * a frame that is too long,
//   * a request that is never answered.
//
// A core that stops reading the pipe stops working: the extensions are gone
// and the editor hangs with them. Every one of these cases has to leave the
// core alive and answering.
// -----------------------------------------------------------------------------

#include "../../core/ipc/IPCClient.h"
#include "../../core/utils/Json.h"
#include "ExtHostPipeClient.h"
#include "HostileRunner.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using vrutti::core::ipc::IPCClient;
using vrutti::core::utils::JsonNode;
using vrutti::core::utils::JsonParser;
using vrutti::tests::HostileRunner;
using vrutti::tests::PipeClient;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& message) {
    if (condition) {
        std::cout << "  [PASS] " << message << "\n";
    } else {
        std::cerr << "  [FAIL] " << message << "\n";
        ++g_failures;
    }
}

// Connects a peer to the pipe of the core and waits until the core has
// noticed the peer.
//
// The peer is connected before the core knows about it: the core learns
// about it in its own thread. A test that sends a frame right away would
// send it into a pipe that nobody is listening to yet.
bool connectPeer(IPCClient& client, PipeClient& peer, const std::string& pipeName) {
    std::atomic<bool> noticed{false};
    client.setOnConnected([&noticed]() { noticed = true; });
    if (!peer.connect(pipeName, 5000)) return false;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline && !noticed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return noticed;
}

// Shows a text with the bytes that cannot be printed made visible.
std::string show(const std::string& text) {
    std::string out;
    for (unsigned char c : text) {
        if (c >= 32 && c < 127) {
            out += static_cast<char>(c);
        } else {
            char hex[8];
            std::snprintf(hex, sizeof(hex), "\\x%02x", c);
            out += hex;
        }
    }
    return out;
}

} // namespace

int main() {
#ifdef _WIN32
    std::cout << "[IpcFuzzTest] The pipe under hostile input.\n\n";
#else
    std::cout << "[IpcFuzzTest] SKIPPED: the named pipe transport is Windows only.\n";
    return 0;
#endif

    // --- the frames that are not JSON -------------------------------------------
    std::cout << "-- the frames that are not JSON\n";
    {
        IPCClient client("vrutti_fuzz_no_pipe");
        std::atomic<int> requests{0};
        client.setOnRequest([&requests](const IPCClient::RpcRequest&) { ++requests; });

        std::mt19937 random(20240918);
        std::uniform_int_distribution<int> byteDistribution(0, 255);
        std::uniform_int_distribution<int> lengthDistribution(0, 300);

        std::vector<std::string> frames;
        frames.reserve(5000);
        for (int i = 0; i < 5000; ++i) {
            const int length = lengthDistribution(random);
            std::string frame;
            frame.reserve(length);
            for (int j = 0; j < length; ++j) {
                frame.push_back(static_cast<char>(byteDistribution(random)));
            }
            frames.push_back(std::move(frame));
        }

        HostileRunner runner;
        for (const auto& frame : frames) {
            // The frame is copied into the work: the text of the loop is
            // gone when the loop is over, and the work runs later.
            runner.push([&client, frame]() { client.handleIncomingMessage(frame); });
        }

        const bool finished = runner.waitIdle(60000);
        const size_t done = runner.done();
        check(finished, "five thousand random frames did not stop the reader (done " +
              std::to_string(done) + ")");
        if (!finished) {
            const size_t index = runner.started() > 0 ? runner.started() - 1 : 0;
            std::cerr << "  [note] the frame that hangs is #" << index << ": ["
                      << show(index < frames.size() ? frames[index] : std::string()) << "]\n";
        }
        check(requests == 0, "no random frame was taken for a request");
    }

    // --- the frames that mean something else -------------------------------------
    std::cout << "\n-- the frames that mean something else\n";
    {
        IPCClient client("vrutti_fuzz_no_pipe_2");
        std::atomic<int> requests{0};
        std::vector<IPCClient::RpcRequest> seen;
        client.setOnRequest([&requests, &seen](const IPCClient::RpcRequest& request) {
            ++requests;
            seen.push_back(request);
        });

        const char* frames[] = {
            "[]",
            "null",
            "42",
            "\"a string\"",
            "{\"type\":42}",
            "{\"type\":\"request\"}",
            "{\"type\":\"request\",\"reqId\":\"not a number\"}",
            "{\"type\":\"request\",\"reqId\":1e300,\"proxyId\":1e300,\"method\":\"x\"}",
            "{\"type\":\"request\",\"reqId\":1.5,\"proxyId\":-99999999999999,\"method\":\"x\"}",
            "{\"type\":\"request\",\"reqId\":1,\"proxyId\":1,\"method\":\"x\",\"args\":\"not an array\"}",
            "{\"type\":\"reply\",\"reqId\":1e300}",
            "{\"type\":\"replyErr\",\"reqId\":1e300,\"err\":42}",
            "{\"type\":\"unknown-frame-type\",\"params\":{\"a\":1}}",
            "{\"type\":\"request\",\"reqId\":1,\"proxyId\":1,\"method\":\"x\",\"args\":[[[[[[[[[[1]]]]]]]]]]}",
        };

        HostileRunner runner;
        for (const char* frame : frames) {
            const std::string text(frame);
            runner.push([&client, text]() { client.handleIncomingMessage(text); });
        }
        const bool finished = runner.waitIdle(30000);
        check(finished, "the frames that mean something else did not stop the reader");
        check(requests == 6,
              "the frames that ask for a request were taken as requests (got " +
              std::to_string(requests.load()) + ")");

        // A number that is out of the range of an integer must not be turned
        // into one: the cast of such a value is undefined behaviour, and the
        // result would be a request number that belongs to another call.
        bool sane = true;
        for (const auto& request : seen) {
            if (request.reqId < 0 || request.reqId > 1000) sane = false;
            if (request.proxyId < -100000 || request.proxyId > 1000) sane = false;
        }
        check(sane, "no request number was turned into a value that is out of range");
    }

    // --- a frame that is cut in half ---------------------------------------------
    std::cout << "\n-- a frame that is cut in half\n";
    {
        IPCClient client("vrutti_fuzz_no_pipe_3");
        const std::string frame =
            "{\"type\":\"request\",\"reqId\":7,\"proxyId\":3,\"method\":\"MainThreadTest.$x\","
            "\"args\":[{\"a\":[1,2,3]},\"text\",true,null]}";

        HostileRunner runner;
        for (size_t length = 0; length <= frame.size(); ++length) {
            const std::string part = frame.substr(0, length);
            runner.push([&client, part]() { client.handleIncomingMessage(part); });
        }
        const bool finished = runner.waitIdle(30000);
        const size_t done = runner.done();
        check(finished, "every half of a frame did not stop the reader (done " +
              std::to_string(done) + ")");
    }

    // --- a call when the host is not there ----------------------------------------
    std::cout << "\n-- a call when the host is not there\n";
    {
        IPCClient client("vrutti_fuzz_not_connected");
        client.start();

        std::atomic<bool> rejected{false};
        std::string reason;
        auto args = std::make_shared<JsonNode>(JsonNode::Type::Array);

        const auto started = std::chrono::steady_clock::now();
        client.sendRequest(1, "$test", args,
            [](const std::shared_ptr<JsonNode>&) {},
            [&rejected, &reason](const std::string& message, const std::string&) {
                reason = message;
                rejected = true;
            },
            30000);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);

        check(rejected, "the call was given up on at once when there is no host");
        check(elapsed.count() < 2000,
              "the caller did not wait for the timeout (after " +
              std::to_string(elapsed.count()) + " ms)");
        check(!reason.empty(), "the reason of the failure is known");
        client.stop();
    }

    // --- a request that is never answered -----------------------------------------
    std::cout << "\n-- a request that is never answered\n";
    {
        const std::string pipeName = "vrutti_fuzz_timeout_" + std::to_string(GetCurrentProcessId());
        IPCClient client(pipeName);
        client.start();

        PipeClient peer;
        const bool connected = connectPeer(client, peer, pipeName);
        check(connected, "the peer connected to the pipe");
        if (connected) {
            std::atomic<bool> rejected{false};
            std::string reason;
            auto args = std::make_shared<JsonNode>(JsonNode::Type::Array);

            const auto started = std::chrono::steady_clock::now();
            client.sendRequest(1, "$test", args,
                [](const std::shared_ptr<JsonNode>&) {},
                [&rejected, &reason](const std::string& message, const std::string&) {
                    reason = message;
                    rejected = true;
                },
                400);

            // The answer never comes: the call has to be given up on.
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (std::chrono::steady_clock::now() < deadline && !rejected) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started);

            check(rejected, "the call that was never answered was given up on");
            check(elapsed.count() < 3000,
                  "the call was given up on near its deadline (after " +
                  std::to_string(elapsed.count()) + " ms)");
            check(!reason.empty(), "the reason of the failure is known");
        }
        peer.disconnect();
        client.stop();
    }

    // --- too many calls at the same time -------------------------------------------
    std::cout << "\n-- too many calls at the same time\n";
    {
        const std::string pipeName = "vrutti_fuzz_flood_" + std::to_string(GetCurrentProcessId());
        IPCClient client(pipeName);
        client.start();

        PipeClient peer;
        const bool connected = connectPeer(client, peer, pipeName);
        check(connected, "the peer connected to the pipe");

        // The peer reads the frames and never answers: the calls stay open.
        std::atomic<bool> draining{true};
        std::thread drainer([&peer, &draining]() {
            while (draining) peer.drainBytes(50);
        });

        std::atomic<int> rejected{0};
        auto args = std::make_shared<JsonNode>(JsonNode::Type::Array);
        const auto reject = [&rejected](const std::string&, const std::string&) { ++rejected; };

        int accepted = 0;
        for (size_t i = 0; i < IPCClient::kMaxPendingCalls + 16; ++i) {
            const int id = client.sendRequest(1, "$test", args, nullptr, reject, 60000);
            if (id >= 0) ++accepted;
        }
        check(accepted == static_cast<int>(IPCClient::kMaxPendingCalls),
              "the table of the open calls stops at its limit (accepted " +
              std::to_string(accepted) + ")");
        check(rejected == 16, "the calls that did not fit were given up on at once");

        draining = false;
        drainer.join();
        peer.disconnect();
        client.stop();
    }

    // --- a frame that never ends ------------------------------------------------------
    std::cout << "\n-- a frame that never ends\n";
    {
        const std::string pipeName = "vrutti_fuzz_overflow_" + std::to_string(GetCurrentProcessId());
        IPCClient client(pipeName);
        client.start();

        PipeClient peer;
        const bool connected = connectPeer(client, peer, pipeName);
        check(connected, "the peer connected to the pipe");
        if (connected) {
            // A frame that never ends: the peer sends bytes without a line
            // break. The core has to notice that the buffer grows without a
            // limit and drop the connection.
            const size_t chunkSize = 1024 * 1024;
            const std::string chunk(chunkSize, 'x');
            size_t sent = 0;
            const size_t limit = IPCClient::kMaxFrameBytes + chunkSize;
            while (sent < limit && peer.connected()) {
                if (!peer.sendBytes(chunk)) break;
                sent += chunk.size();
            }

            // The core drops the connection: the pipe is closed.
            bool dropped = false;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            while (std::chrono::steady_clock::now() < deadline) {
                if (!peer.connected()) { dropped = true; break; }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            check(dropped, "the core dropped the connection of the frame that never ends");
        }
        peer.disconnect();
        client.stop();
    }

    // --- a frame that is too long -------------------------------------------------------
    std::cout << "\n-- a frame that is too long\n";
    {
        const std::string pipeName = "vrutti_fuzz_toolong_" + std::to_string(GetCurrentProcessId());
        IPCClient client(pipeName);
        client.start();

        std::atomic<int> requests{0};
        client.setOnRequest([&requests](const IPCClient::RpcRequest&) { ++requests; });

        PipeClient peer;
        const bool connected = connectPeer(client, peer, pipeName);
        check(connected, "the peer connected to the pipe");
        if (connected) {
            // A frame that is too long but is complete: it has a line break
            // at its end. The line break used to be taken off before the
            // size was looked at, so such a frame was read after all.
            std::string frame = "{\"type\":\"request\",\"reqId\":1,\"proxyId\":1,"
                                "\"method\":\"$x\",\"args\":[],\"padding\":\"";
            frame.append(IPCClient::kMaxFrameBytes + 1024, 'p');
            frame += "\"}\n";

            const size_t chunkSize = 1024 * 1024;
            size_t sent = 0;
            while (sent < frame.size() && peer.connected()) {
                const size_t part = std::min(chunkSize, frame.size() - sent);
                if (!peer.sendBytes(frame.substr(sent, part))) break;
                sent += part;
            }

            bool dropped = false;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            while (std::chrono::steady_clock::now() < deadline) {
                if (!peer.connected()) { dropped = true; break; }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            check(dropped, "the core dropped the connection of the frame that is too long");
            check(requests == 0, "the frame that is too long was not handed to a handler");
        }
        peer.disconnect();
        client.stop();
    }

    // --- the client that goes away ------------------------------------------------------
    std::cout << "\n-- the client that goes away\n";
    {
        // A client that is thrown away while its thread is running used to
        // leave that thread behind: the thread reads the members of an
        // object that is gone.
        bool survived = true;
        for (int i = 0; i < 20; ++i) {
            try {
                IPCClient client("vrutti_fuzz_lifetime_" + std::to_string(i));
                client.start();
                client.stop();
            } catch (const std::exception&) {
                survived = false;
                break;
            }
        }
        check(survived, "a client can be started and thrown away again and again");
    }

    std::cout << "\n";
    if (g_failures > 0) {
        std::cerr << "[IpcFuzzTest] FAILED (" << g_failures << " assertion(s))\n";
        return 1;
    }
    std::cout << "[IpcFuzzTest] OK: the pipe survives hostile input\n";
    return 0;
}
