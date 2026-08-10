<div align="center">
  
# 🌌 Vrutti IDE

**A next-generation, high-performance development environment built from the ground up.**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform: Windows](https://img.shields.io/badge/platform-Windows-lightgrey)](#)
[![Tech: C++](https://img.shields.io/badge/backend-C++-00599C?logo=c%2B%2B)](#)
[![Tech: Lit](https://img.shields.io/badge/frontend-Lit-324FFF)](#)

*Experience buttery-smooth native performance combined with modern web aesthetics.*

<!-- Replace with a real screenshot later -->
<img src="logos/vrutti-logo.png" width="400" alt="Vrutti Logo">

</div>

---

## ✨ Features

- 🧬 **Topological Git Visualizer**: Explore your commit history with a buttery-smooth, hardware-accelerated horizontal SVG canvas. Features drag-to-pan, interactive zooming, dynamic routing, reflog parsing, and direct git actions (Revert, Delete, Rename) right from the UI!
- 🎨 **Sleek Interface**: Built with **Lit Web Components**, providing a modern, fast, and highly customizable user interface featuring pastel colors, sleek dark modes, and dynamic micro-animations.
- ⚡ **Fast Architecture**: Robust C++ native backend communicating with a nimble Node.js/Lit frontend via IPC, keeping memory footprint low and UI responsiveness incredibly high.
- 🛠 **Integrated Tasks**: Built-in task runner supporting custom `tasks.json` configuration natively tied to the embedded terminal.
- 🧩 **Built-in Extensions**: Out-of-the-box syntax highlighting and smart completion powered by the embedded CodeMirror engine.

---

## 🚀 Installation

Vrutti is now available as a packaged application for Windows!

### 📥 Download the Setup Installer
*(Note: If you don't have the installer yet, see [Building the Installer](#building-the-installer) below)*

1. Go to the [Releases](https://github.com/UnboundSB/Vrutti/releases) page.
2. Download the latest `Vrutti_Setup.exe`.
3. Double click the installer and follow the prompt.
4. Launch **Vrutti IDE** from your Desktop or Start Menu!

---

## 🛠 Developer Guide

### Building from Source

To build the IDE locally:
1. Ensure you have **Docker**, **CMake**, and a **C++ compiler** installed.
2. Clone the repository: `git clone https://github.com/UnboundSB/Vrutti.git`
3. Build the frontend via Docker: `docker compose -f docker-compose.frontend.yml up`
4. Build the backend C++ application using CMake.
5. Run the generated `build/vrutti_app.exe`!

### Building the Installer

Want to package the installer yourself?
1. Ensure you have [Inno Setup 6](https://jrsoftware.org/isdl.php) installed on your Windows machine.
2. Build the C++ backend and the frontend via Docker.
3. Run the packaging script:
   ```powershell
   .\build_installer.ps1
   ```
4. The script will generate `Vrutti_Setup.exe` in the root folder.

---

## 📜 License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

<div align="center">
  <i>Built with ❤️ for modern developers.</i>
</div>
