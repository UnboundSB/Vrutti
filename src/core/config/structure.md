# Core Config Module

This directory contains the global configuration backend for Vrutti IDE.

## Architecture

The Vrutti IDE settings are managed by a purely native C++ module rather than relying on the frontend. This ensures settings persist and load instantly before the webview is even fully initialized.

- `SettingsManager.h / .cpp`:
  - Implements a thread-safe singleton that manages the parsed `settings.json` tree in memory.
  - Automatically loads from `%APPDATA%\Vrutti\settings.json` (on Windows) or `~/.config/Vrutti/settings.json` on Unix.
  - Exposes `getSettingsJson()` and `updateSetting()` methods which are tightly bound to the native `WebView2` JS IPC layer.
  - Background tasks (via `std::thread` / `ThreadPool`) ensure disk writes are asynchronous and completely non-blocking to the main event loop.
