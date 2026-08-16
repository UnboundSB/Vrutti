# User Interface (src/ui/)

## Purpose
Manages all presentation layers of the IDE, including the native OS window management and the rich HTML/JS/CSS frontend editor.

## Files
- (No immediate source files)

## Child Directories
- **compositor/**: Native C++ code that creates OS-level windows (using Win32/Webview2), handles window chrome, resizing, and hosts the webview.
- **frontend/**: The web application (HTML/CSS/TypeScript) that renders the actual IDE interface (editor, explorer, search sidebar).
- **vendor/**: Third-party native headers or dependencies used directly by the UI (like webview.h).
