# Frontend Source (src/ui/frontend/src/)

## Purpose
The main TypeScript source code directory for the frontend application. It initializes the UI and manages global states and themes.

## Files
- **main.ts**: The main entry point for the frontend logic. It sets up IPC listeners, requests initial configuration from the native backend, and renders the root <vrutti-editor-layout> component.
- **index.css**: Global CSS styles, theme variables, and reset rules.
- **vite-env.d.ts**: Type declarations for Vite environment variables.

## Child Directories
- **components/**: Contains all reusable Web Components (Lit) that make up the IDE layout, such as the explorer, terminal, and search bar.


## Additional Components
- **globalHover.ts**: Component implementation.
- **shared-styles.ts**: Component implementation.
- **ThemeBridge.ts**: Component implementation.

