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
