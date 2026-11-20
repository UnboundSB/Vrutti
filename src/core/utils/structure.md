# Utilities (src/core/utils/)

## Purpose
Contains shared helpers and standard abstractions used globally across the C++ codebase.

## Files
- **JsonParser.cpp**: A fast, custom JSON parser used for decoding IPC messages and configuration files without heavy third-party dependencies.
- **JsonParser.h**: Header for the JsonParser class.
- **JsonSerializer.h**: Utility to serialize C++ objects and strings into valid JSON strings.

## Child Directories
- (No immediate subdirectories)


## Additional Components
- **Base64.h**: Component implementation.
- **Json.cpp**: Component implementation.
- **Json.h**: Component implementation.
- **LineScanner.cpp**: Component implementation.
- **LineScanner.h**: Component implementation.
- **Paths.cpp**: Resolves paths inside the Vrutti installation from the executable location.
- **Paths.h**: Header for the installation path helpers.
- **StringPool.cpp**: Component implementation.
- **StringPool.h**: Component implementation.

## Public Methods
- **JsonParser::parse(source)**: Parses JSON text into a DOM tree; the tree owns its source and decodes escape sequences once.
- **JsonParser::unescapeString(text)**: Resolves JSON escape sequences; unknown escapes keep their backslash.
- **JsonSerializer::stringify(node, indentLevel, pretty)**: Serializes a DOM tree back into JSON text.
- **JsonSerializer::escapeString(text)**: Escapes a string into a quoted JSON literal.
- **JsonNode::get(key)**: Returns the property of an object node, or null.
- **makeJsonString(value)**: Creates a string node that owns its text.
- **appBasePath()**: Returns the root directory of the Vrutti installation.
- **appResourcePath(relative)**: Returns the path of a resource inside the installation.

