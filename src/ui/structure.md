# UI Module Structure

This folder contains the native rendering interface for Vrutti IDE. We use a GPU-accelerated approach (ImGui + OpenGL3) to ensure the editor draws text extremely fast without blocking the system.

## Sub-Modules

### `compositor/`
Handles the core OS-level window rendering and hardware context.
* **`Window.h/cpp`**: Initializes OpenGL, handles main window creation, and manages the main render loop (input polling, frame swapping).

### `views/`
High-level structural panels that make up the IDE layout.
* **`Layout.h/cpp`**: The overarching DockSpace layout engine. It splits the screen logically and anchors the Sidebar and Editor Tabs.
* **`FileExplorer.h/cpp`**: Renders the workspace directory tree. Interfaces directly with the lazy-loading `Workspace`.
* **`EditorView.h/cpp`**: Visually renders the actual text content and code buffers (PieceTable).

### `widgets/`
Reusable, lightweight UI components.
* **`TabBar.h/cpp`**: Renders the row of open file tabs at the top of the editor.

## Subdirectories

* **`vendor/`**: Contains third-party dependencies required for the UI to function (e.g., Dear ImGui).