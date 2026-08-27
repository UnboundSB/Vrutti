# UI Components (src/ui/frontend/src/components/)

## Purpose
Houses the modular Web Components (built with the Lit library) that construct the various panels and interactive elements of the IDE.

## Files
- **vrutti-editor-layout.ts**: The root layout component defining the structural grid (Activity Bar, Sidebar, Editor, Panel, Status Bar).
- **vrutti-search.ts**: The search sidebar component that triggers IPC search commands and renders results.
- **vrutti-settings.ts**: The settings panel component.
- **vrutti-terminal.ts**: Integrates xterm.js for the integrated terminal view.

## Child Directories
- **explorer/**: Components specifically related to the file explorer tree view.
- **scm/**: Source control management components (e.g., Git integration).


## Additional Components
- **codicons.ts**: SVG icon definitions used across the IDE.
- **vrutti-context-menu.ts**: Implements the right-click context menu system.
- **vrutti-debug-console.ts**: The debug console output and REPL view.
- **vrutti-debug-sidebar.ts**: The debug sidebar containing variables, watch, call stack, and breakpoints.
- **vrutti-editor.ts**: Code editor wrapper integrating CodeMirror 6.
- **vrutti-explorer-view.ts**: The file explorer tree view.
- **vrutti-extension-details.ts**: View for rendering details of a selected extension.
- **vrutti-extensions.ts**: The extensions marketplace and local management view.
- **vrutti-menubar.ts**: The top title bar and application menu.
- **vrutti-output-view.ts**: The output panel for logs and extension output.
- **vrutti-panel.ts**: The bottom panel container (Terminal, Output, Debug Console, etc).
- **vrutti-quick-pick.ts**: The command palette and quick input overlay.
- **vrutti-sidebar.ts**: The main sidebar container which includes the embedded Activity Bar and view panels.
- **vrutti-statusbar.ts**: The bottom status bar showing editor and workspace information.
- **vrutti-task-manager.ts**: Manages the running tasks and their processes.
- **vrutti-terminal-view.ts**: Terminal view implementation.
- **vrutti-webview.ts**: Wrapper for custom webviews rendered inside the IDE.
- **xterm-css.ts**: CSS styling for xterm.js integration.
- **xterm-styles.ts**: Additional styles for xterm.js.

## Planned Components & Roadmap
The following components are slated for future implementation to achieve parity with modern IDE architectures. Here is the current implementation status:
1. **vrutti-problems-view.ts (Problems / Diagnostics Panel)**: Exists: No, Dynamic: No
2. **vrutti-editor-layout.ts (Split Editors)**: Exists: Yes, Dynamic: Yes
3. **vrutti-search.ts (Search & Replace)**: Exists: Yes, Dynamic: Yes
4. **LSP (Language Server Protocol)**: Exists: No, Dynamic: No
5. **vrutti-diff-editor.ts (Diff Editor)**: Exists: No, Dynamic: No
6. **Minimap & Breadcrumbs**: Exists: No, Dynamic: No

- **vrutti-dynamic-background.ts**: A lightweight, cache-flushed dynamic background rendering engine to support custom images/videos behind the IDE while optimizing memory on file open.
