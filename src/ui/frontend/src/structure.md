# src (src/ui/frontend/src/)

## Purpose
The renderer sources: the workbench components, the services and the entry point.

## Files
- **dom-bridge.ts**: dom-bridge.ts
- **globalHover.ts**: TypeScript module: globalHover.
- **main.ts**: The `VruttiApp` module.
- **shared-styles.ts**: TypeScript module: shared styles.
- **ThemeBridge.ts**: ThemeBridge
- **TODO.md**: Open questions and planned work for this folder.

## Public Methods
- **installDomBridge()**
- **VruttiApp.disconnectedCallback()**
- **VruttiApp.render()**
- **globalHoverStyle()**
- **themeBridge()**

## Child Directories
- **components/**: The renderer components.
- **core/**: Frontend core: contribution registry, core contributions, and service layer. The `Registry.ts` is Vrutti's hand-rolled contribution system (modeled loosely on...
- **services/**: The renderer services: the bridge to the core, the theme applier and the workspace model.
