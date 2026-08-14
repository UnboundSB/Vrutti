<div align="center">

<img src="https://raw.githubusercontent.com/UnboundSB/Vrutti/main/logos/logo-512x512.png" width="120" alt="Vrutti Logo">

# Vrutti IDE

**A next-generation, high-performance development environment built from the ground up.**

| | | | |
|:---:|:---:|:---:|:---:|
| [![Status](https://img.shields.io/badge/Status-Active-success)](#) | [![License](https://img.shields.io/badge/License-MIT-yellow)](LICENSE) | [![Platform](https://img.shields.io/badge/Platform-Windows-blue)](#) | [![Engine](https://img.shields.io/badge/Engine-C%2B%2B-00599C)](#) |

</div>

---

## Downloads

| Package | Best For | Requirements | Link |
|---|---|---|---|
| **Vrutti_Setup.exe** | Most users — installs shortcuts, registers file associations, auto-updates | Windows 10/11, Admin rights | [Download](https://github.com/UnboundSB/Vrutti/releases/latest/download/Vrutti_Setup.exe) |
| **vrutti_early_release.zip** | Portable use, USB drives, no-admin environments | Windows 10/11 | [Download](https://github.com/UnboundSB/Vrutti/releases/latest/download/vrutti_early_release.zip) |

No Docker, CMake, or compilers are required to run Vrutti — both packages are pre-built and ready to launch. Older versions are available on the [Releases page](https://github.com/UnboundSB/Vrutti/releases).

---

## At a Glance

| Category | Detail |
|---|---|
| Backend | Native C++ core |
| Frontend | Node.js + Lit Web Components |
| Communication | IPC between backend and frontend |
| Syntax Highlighting | CodeMirror |
| Task Running | Native `tasks.json` support |
| Version Control | Built-in topological Git visualizer |
| Supported OS | Windows 10 / 11 |
| License | MIT |

---

## Features

| Feature | Description |
|---|---|
| **Lightning Fast Engine** | Native C++ backend communicates with the Lit/Node.js frontend over IPC, keeping memory footprint low and UI responsiveness high. |
| **Topological Git Visualizer** | Hardware-accelerated horizontal SVG canvas for commit history. Drag-to-pan, interactive zoom, dynamic routing, reflog parsing, and direct git actions (revert, delete, rename) from the UI. |
| **Sleek Interface** | Lit Web Components power a modern, customizable UI with pastel and dark themes plus subtle micro-animations. |
| **Integrated Task Runner** | Native `tasks.json` configuration tied directly to the embedded terminal. |
| **Syntax Highlighting** | Powered by CodeMirror out of the box. |

---

## Quick Start

| Step | Action |
|:---:|---|
| 1 | Download the installer or portable ZIP from the table above |
| 2 | Run the installer, or extract the ZIP to a folder of your choice |
| 3 | Launch `vrutti_app.exe` |
| 4 | Open a project folder and start working — no configuration required |

---

## Building From Source

The pre-built downloads above are all most users need. The steps below are only for contributors modifying Vrutti's source.

### Build the Core

| Step | Command / Action |
|:---:|---|
| 1 | Install **Docker**, **CMake**, and a C++ compiler |
| 2 | `git clone https://github.com/UnboundSB/Vrutti.git` |
| 3 | `docker compose -f docker-compose.frontend.yml up` (builds frontend) |
| 4 | Build the backend C++ application using CMake |
| 5 | Run `build/vrutti_app.exe` |

### Build a Release Package

| Step | Command / Action |
|:---:|---|
| 1 | Install [Inno Setup 6](https://jrsoftware.org/isdl.php) (required for EXE packaging) |
| 2 | Build the C++ backend and frontend (see steps above) |
| 3 | `.\package_release.ps1` → produces the portable ZIP |
| 4 | `.\build_installer.ps1` → produces `Vrutti_Setup.exe` |

Both scripts output their artifacts to the project root.

---

## Contributing

| | |
|---|---|
| **Status** | Open for contributions |
| **Process** | Fork → build → submit a pull request |
| **Recognition** | Merged contributions are added to the contributors list |
| **Links** | [Fork](https://github.com/UnboundSB/Vrutti/fork) · [Issues](https://github.com/UnboundSB/Vrutti/issues) · [Pull Requests](https://github.com/UnboundSB/Vrutti/pulls) |

---

## License

Licensed under the MIT License. See [LICENSE](LICENSE) for full details.

<div align="center">

Built for developers who want a fast, modern editor without the setup overhead.

</div>
