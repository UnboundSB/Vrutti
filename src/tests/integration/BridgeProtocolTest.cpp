// -----------------------------------------------------------------------------
// The bridge between the page and the core.
//
// The page of the editor talks to the core through one channel. The page
// is not a trusted caller, so every message of the page is checked before
// the core does anything with it:
//
//   * a message that is too big is not read,
//   * a method that the core does not know is dropped,
//   * a message whose parameters are not the shape that the core expects
//     is dropped.
//
// The proof is here: the rules live in `core/ipc/BridgeProtocol.cpp` and
// they are checked both as a rule and through the real router.
// -----------------------------------------------------------------------------

#include "../../core/ipc/BridgeProtocol.h"
#include "../../core/ipc/IPCClient.h"
#include "../../core/ipc/VSCodeProtocol.h"

#include <iostream>
#include <string>
#include <vector>

using vrutti::core::ipc::IPCClient;
using vrutti::core::ipc::VSCodeProtocol;
using vrutti::core::utils::JsonNode;
using vrutti::core::utils::JsonParser;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (condition) {
        std::cout << "  [PASS] " << what << "\n";
    } else {
        std::cerr << "  [FAIL] " << what << "\n";
        ++g_failures;
    }
}

std::shared_ptr<JsonNode> object(const std::string& text) {
    return JsonParser::parse(text);
}

} // namespace

int main() {
    std::cout << "[BridgeProtocolTest] The bridge between the page and the core.\n\n";

    using namespace vrutti::core::ipc;

    // --- the name of a method has to be a name --------------------------------
    std::cout << "-- the name of a method has to be a name\n";
    {
        check(isSaneMethodName("theme/set"), "a normal name is a name");
        check(!isSaneMethodName(""), "an empty name is not a name");
        check(!isSaneMethodName(std::string(300, 'x')),
              "a name that is endless is not a name");
        check(!isSaneMethodName(std::string("theme\nset")),
              "a name with a line break in it is not a name");
        check(!isSaneMethodName(std::string("theme/set\x01")),
              "a name with a control character in it is not a name");
    }

    // --- the page may only call the methods that the core knows -------------
    std::cout << "\n-- the page may only call the methods that the core knows\n";
    {
        // Every method that the page of the editor sends has to be on the
        // list. If one of them is missing, the feature is broken -- and the
        // test says so.
        const std::vector<std::string> methodsOfThePage = {
            "extensions/activateEvent",
            "extensions/request_installed",
            "extensions/install",
            "extensions/uninstall",
            "extensions/enable",
            "extensions/disable",
            "themes/available",
            "icon_themes/available",
            "theme/set",
            "icon_theme/set",
            "command/execute",
            "debug/evaluate",
            "debuggers/available",
            "dap/start",
            "dap/stop",
            "dap/request",
            "editor/run",
            "treeview/request",
            "webviewView/resolve",
            "webview/receiveMessage",
        };
        bool allAllowed = true;
        for (const auto& method : methodsOfThePage) {
            if (!isAllowedBridgeMethod(method)) {
                std::cerr << "    the page sends '" << method << "', and it is not allowed\n";
                allAllowed = false;
            }
        }
        check(allAllowed, "every method that the page sends is allowed");

        check(!isAllowedBridgeMethod("rm -rf /"),
              "a method that the core does not know is not allowed");
        check(!isAllowedBridgeMethod("extensions/installEverything"),
              "a method that only looks like a method is not allowed");
        check(!isAllowedBridgeMethod(""),
              "an empty name is not allowed");
    }

    // --- a message that is too big is not read -------------------------------
    std::cout << "\n-- a message that is too big is not read\n";
    {
        const std::string small = "[\"theme/set\",\"{}\"]";
        check(parseBridgeMessage(small) != nullptr, "a small message is read");

        const std::string huge(kMaxBridgeMessageBytes + 1, 'p');
        check(parseBridgeMessage(huge) == nullptr, "a message that is too big is not read");

        check(parseBridgeMessage("not json at all") == nullptr,
              "a message that is not JSON is not read");
        check(parseBridgeMessage("{\"method\":\"theme/set\"}") == nullptr,
              "a message that is not an array of two things is not read");
        check(parseBridgeMessage("[\"theme/set\"]") == nullptr,
              "a message without a payload is not read");
        check(parseBridgeMessage("[42,\"{}\"]") == nullptr,
              "a message whose method is not a text is not read");

        check(parseBridgePayload("{\"name\":\"Dark\"}") != nullptr,
              "a payload that holds an object is read");
        check(parseBridgePayload("not json") == nullptr,
              "a payload that does not hold an object is not read");
        check(parseBridgePayload("[\"Dark\"]") == nullptr,
              "a payload that holds a list is not read");
    }

    // --- the shape of the parameters ------------------------------------------
    std::cout << "\n-- the shape of the parameters\n";
    {
        check(checkBridgeParams("theme/set", object("{\"name\":\"Dark\"}")),
              "a theme with a name is a theme");
        check(checkBridgeParams("theme/set", object("{}")),
              "a theme without a name is left to the core");
        check(!checkBridgeParams("theme/set", object("{\"name\":42}")),
              "a theme whose name is a number is refused");
        check(!checkBridgeParams("theme/set", object("[\"Dark\"]")),
              "parameters that are not an object are refused");

        check(checkBridgeParams("command/execute", object("{\"command\":\"workbench.action.files.save\"}")),
              "a command with a name is a command");
        check(!checkBridgeParams("command/execute", object("{\"command\":{\"name\":\"x\"}}")),
              "a command whose name is an object is refused");

        check(checkBridgeParams("extensions/activateEvent", object("{\"event\":\"*\"}")),
              "an event with a name is an event");
        check(!checkBridgeParams("extensions/activateEvent", object("{\"event\":7}")),
              "an event whose name is a number is refused");

        check(checkBridgeParams("dap/request", object("{\"command\":\"threads\",\"args\":{}}")),
              "a call of the debugger is a call");
        check(!checkBridgeParams("dap/request", object("{\"command\":\"threads\",\"args\":5}")),
              "a call of the debugger whose arguments are a number is refused");

        check(checkBridgeParams("treeview/request",
                                object("{\"reqId\":\"12\",\"viewId\":\"v\",\"method\":\"getChildren\",\"nodeHandle\":null}")),
              "a request of a tree view with no node is a request");
        check(!checkBridgeParams("treeview/request", object("{\"reqId\":[1]}")),
              "a request of a tree view whose number is a list is refused");

        check(checkBridgeParams("extensions/request_installed", object("{}")),
              "a request of the list of the extensions takes nothing");
    }

    // --- the router of the core obeys the rules -------------------------------
    std::cout << "\n-- the router of the core obeys the rules\n";
    {
        IPCClient client("vrutti_bridge_test_no_pipe");
        std::vector<std::string> events;
        client.setOnEvent([&events](const std::string& method,
                                    const std::shared_ptr<JsonNode>&) {
            events.push_back(method);
        });

        VSCodeProtocol router(&client);

        // A method that the core does not know reaches nobody.
        router.handleLocalRequest("rm -rf /", object("{}"));
        check(events.empty(), "a method that the core does not know reaches nobody");

        // A message with the wrong shape reaches nobody.
        router.handleLocalRequest("theme/set", object("{\"name\":42}"));
        check(events.empty(), "a message with the wrong shape reaches nobody");

        // A method that the core knows, with the right shape, is handed over
        // to the interface: the debugger calls are the example.
        router.handleLocalRequest("dap/request",
                                  object("{\"command\":\"threads\",\"args\":{}}"));
        check(events.size() == 1 && events[0] == "dap/request",
              "a message with the right shape reaches the interface");

        // The name of a method that is not a name is never even looked at.
        router.handleLocalRequest(std::string("dap\n/request"), object("{}"));
        check(events.size() == 1,
              "a name that is not a name reaches nobody");
    }

    std::cout << "\n";
    if (g_failures > 0) {
        std::cerr << "[BridgeProtocolTest] FAILED (" << g_failures << " of "
                  << g_checks << " checks)\n";
        return 1;
    }
    std::cout << "[BridgeProtocolTest] OK: " << g_checks << " checks\n";
    return 0;
}
