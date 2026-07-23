# Frontend Components Structure

This directory contains reusable Web Components built with Lit for the Vrutti IDE frontend. 
The components are designed using the Shadow DOM to encapsulate styles and behavior, ensuring a clean, modern architecture that doesn't conflict with global styles.

## Directory Layout

- `vrutti-sidebar.ts` - The main sidebar docking overlay. Controls the activity bar (tool icons) and coordinates which side panel (e.g., explorer, search) is currently active.
- `vrutti-statusbar.ts` - The bottom status bar component. Displays critical workspace info such as Git branch, error/warning counts, cursor position, and language modes.
- `codicons.ts` - Contains all the inline SVG definitions used for the tool icons (adapted from VS Code's codicon library).
- `explorer/` - The File Explorer component module, responsible for visualizing the workspace file tree.

## Architecture Guidelines

- All new components must be defined as standard custom elements using `@customElement`.
- Use `:host` styles to define the layout container for the component.
- Limit the use of global state. Complex data should be passed down via properties (`@property`) or injected via dedicated models.
