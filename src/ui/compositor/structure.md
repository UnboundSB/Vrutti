# UI Compositor Module

This folder contains the native C++ window manager layer for Vrutti IDE. 
Instead of drawing graphics manually via OpenGL, this layer initializes an embedded OS webview (WebView2 on Windows) and hosts the Lit web frontend.

## Files
- `Window.h / Window.cpp`: 
  - Initializes the native OS window (HWND).
  - Uses Win32 `WM_NCCALCSIZE` subclassing to create a fully frameless window that retains native resize borders and DWM shadows.
  - Spawns the `webview::webview` instance and navigates it to the bundled frontend HTML.
  - Bridges JavaScript IPC commands directly to native C++ APIs (`minimizeWindow`, `maximizeWindow`, `closeWindow`, `startWindowDrag`).
