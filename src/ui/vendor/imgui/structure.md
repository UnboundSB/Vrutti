# Dear ImGui Structure

This folder contains the core files for the Dear ImGui library. It is designed to be bloat-free and provides immediate-mode graphical interfaces.

## Root Files

* **`imgui.h` / `imgui.cpp`**: These are the main library files that provide the core functionality, window management, and UI logic for ImGui.
* **`imgui_draw.cpp`**: Handles the optimized vertex/index buffer generation to draw the UI shapes.
* **`imgui_widgets.cpp`**: Contains the implementations for standard UI elements like buttons, checkboxes, sliders, and text inputs.
* **`imgui_tables.cpp`**: Implements advanced table and column formatting components for the UI.
* **`imgui_internal.h`**: Contains internal structs and helpers.
* **`imconfig.h`**: A configuration file used to compile ImGui with custom settings.
* **`imstb_rectpack.h`, `imstb_textedit.h`, `imstb_truetype.h`**: Built-in helper libraries for text rendering and packing texture atlases (so fonts can be drawn quickly).

## Subdirectories

* **`backends/`**: Contains the platform-specific bridge code to connect ImGui's abstract drawing commands to real operating systems and graphics cards. See its internal `structure.md` for details.
