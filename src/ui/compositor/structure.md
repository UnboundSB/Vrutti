# UI Compositor (src/ui/compositor/)

## Purpose
The native C++ windowing layer. It establishes the main OS application window, integrates the WebView (e.g., WebView2 on Windows) to render the HTML/JS frontend, and provides bindings for the frontend to communicate with the C++ engine.

## Files
- **Window.cpp**: Contains the platform-specific window creation logic, event loop handling, window dragging (Win32), and WebView2 integration.
- **Window.h**: Header for the Window class.

## Child Directories
- (No immediate subdirectories)
