# compositor (src/ui/compositor/)

## Purpose
The native compositor that owns the window and the webview.

## Files
- **TODO.md**: Open questions and planned work for this folder.
- **Window.cpp**: The encoding of the binary payloads that travel over the bridge.
- **Window.h**: Header for the Window component.

## Public Methods
- **Window::Window(int, int, const std::string&, vrutti::core::ipc::IPCClient*, const std::string&)**
- **Window::~Window()**
- **bool Window::init()**
- **Window::run()**
- **Window::shutdown()**
- **bool Window::shouldClose()**
- **Window::logToOutput(const std::string&, const std::string&)**
