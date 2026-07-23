# Explorer Component Structure

This directory contains the logic and view components for the Vrutti IDE File Explorer sidebar panel.

## Files

- `vrutti-explorer.ts` - The primary Lit Web Component (`<vrutti-explorer>`). This component is inherently recursive; it renders a tree node and then calls itself to render any children (directories).
- `explorerModel.ts` - The underlying data model. Defines `ExplorerItem` (which tracks directory expansion states, file names, etc.) and `ExplorerModel` (the root structure provider). Currently stubbed with static data, but intended to bridge to the C++ core via IPC.
- `iconMapper.ts` - Utility module that maps file names and extensions to their corresponding material-design SVG paths.

## Design

The Explorer is built to mirror standard IDE paradigms:
- Smooth collapse/expand toggles for directories.
- Dynamic localized SVG icon rendering.
- Incremental indentation based on directory depth (`this.depth`).
