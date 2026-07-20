# UI Module Structure

This folder contains the native rendering interface for Vrutti IDE. We use a GPU-accelerated approach to ensure the editor draws text extremely fast without blocking the system.

## Files and Responsibilities

* **`Window.h` / `Window.cpp`**: 
  - **Function**: Handles creating the main application window on your operating system.
  - **Details**: It initializes the OpenGL graphics context, sets up the window frame, and captures system-level mouse and keyboard events. It also manages the main render loop, ensuring the screen is updated continuously.

* **`EditorView.h` / `EditorView.cpp`**: 
  - **Function**: Responsible for drawing the text and user interface components.
  - **Details**: It translates the internal text data (from the Piece Table) into visual pixels on the screen. It uses lazy loading (viewport virtualization), which means it only queries computer memory for the specific lines of text that are currently visible on your screen. This allows you to open massive files instantly because invisible text is never loaded into RAM.