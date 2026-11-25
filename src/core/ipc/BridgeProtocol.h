// -----------------------------------------------------------------------------
// BridgeProtocol -- the rules of the bridge between the page and the core.
//
// The page of the editor talks to the core through one channel: the method
// `sendIpcMessage` of the window. Everything that the page sends goes
// through here first.
//
// The page is not a trusted caller. It may be the page of the editor, or
// the page of a panel that an extension filled with text of its own. A
// message of the page may therefore not be:
//
//   * bigger than the memory that the core is willing to give to it,
//   * a method that the core does not know,
//   * a method whose parameters are not the shape that the core expects.
//
// A message that breaks one of these rules is dropped and written into the
// log. It is never answered with an error: the rule of the editor is that
// a missing feature is answered with the benign default of its return
// type, and the page does not wait for an answer at all.
// -----------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "core/utils/Json.h"

namespace vrutti::core::ipc {

    using vrutti::core::utils::JsonNode;

    // The biggest message that the page may send.
    //
    // The message is read into the memory of the core before it is looked
    // at, so the limit cannot save the memory of the read. It does stop the
    // page from making the core copy the message again and again, and it
    // keeps the size of the work of the core in the hands of the core.
    constexpr size_t kMaxBridgeMessageBytes = 32u * 1024u * 1024u;

    // The longest name of a method of the page.
    constexpr size_t kMaxBridgeMethodLength = 256;

    // The longest text inside a message of the page: a path, a name of a
    // theme, a command, an expression.
    constexpr size_t kMaxBridgeTextLength = 64u * 1024u;

    // Tells whether the name of a method is a name.
    //
    // The name is used as the key of a message and ends up in the log, so
    // it may not be empty, may not be endless and may not hold a control
    // character.
    bool isSaneMethodName(const std::string& method);

    // Tells whether the page may call this method.
    //
    // The list is the contract between the page and the core: every method
    // that the page sends is on it, and nothing else is.
    bool isAllowedBridgeMethod(const std::string& method);

    // Reads a message of the page.
    //
    // A message of the page is an array of two things: the name of the
    // method and the payload of the method as a text.
    //
    // Returns nothing when the message is too big, is not JSON at all, or
    // does not have that shape.
    std::shared_ptr<JsonNode> parseBridgeMessage(const std::string& raw);

    // Reads the payload of a message of the page.
    //
    // The payload is a text that holds a JSON object. Returns nothing when
    // the text is too big or does not hold an object.
    std::shared_ptr<JsonNode> parseBridgePayload(const std::string& text);

    // Checks the shape of the parameters of a method.
    //
    // The parameters of every method of the page are an object. A field
    // that is named has to be of the type that the core expects, and a
    // text may not be longer than its limit.
    bool checkBridgeParams(const std::string& method,
                           const std::shared_ptr<JsonNode>& params);

} // namespace vrutti::core::ipc
