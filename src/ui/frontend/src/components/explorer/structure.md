# Vrutti IDE Explorer Component Structure

This directory contains the File Explorer UI components for Vrutti IDE.

## Files

- `explorerModel.ts`: The data model and state management for the file explorer tree. It communicates asynchronously with the native C++ backend (`vruttiReadDirectory` via `webview::bind`) to dynamically lazily-load folder contents on expansion. It handles data parsing, sorting, and state representation (`isExpanded`, `childrenLoaded`).
- `iconMapper.ts`: A utility module that maps file names and extensions to corresponding icon SVGs for visual representation.
- `vrutti-explorer.ts`: The recursive Lit web component (`<vrutti-explorer>`) that visually renders the tree structure, using `ExplorerItem` objects to dictate rendering logic and nesting.

## Architecture

The File Explorer architecture bridges directly to the native OS filesystem via IPC. By using dynamic, asynchronous recursive loading, it maintains zero footprint on startup, only allocating DOM nodes and querying the OS for directories that the user explicitly expands.
