<div align="center">
  
# 🌌 **V R U T T I &nbsp;&nbsp;I D E** 🌌

**A next-generation, high-performance development environment built from the ground up.**
**We are open for contribution if you want to make the ide better please feel free to make it we will merge it and add your name to contributor's list**
<img src="https://raw.githubusercontent.com/UnboundSB/Vrutti/main/logos/logo-512x512.png" width="350" alt="Vrutti Logo">

<br/>

[![Build Status](https://img.shields.io/badge/Status-Active-success?style=for-the-badge&logo=appveyor)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](LICENSE)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows%20%E2%96%A0-blue?style=for-the-badge)](#)
[![Tech: C++](https://img.shields.io/badge/Engine-C++-00599C?style=for-the-badge&logo=c%2B%2B)](#)
[![Tech: Lit](https://img.shields.io/badge/UI-Lit%20Web%20Components-324FFF?style=for-the-badge&logo=lit)](#)

*Experience buttery-smooth native performance combined with modern web aesthetics.*

<br/>

<h3>🚀 <b>Download & Play (No Setup Required!)</b></h3>

Vrutti is pre-packaged and ready to go. You do NOT need Docker, CMake, or any compilers to run Vrutti. Just click one of the buttons below to download and launch!

<br/>

<a href="https://github.com/UnboundSB/Vrutti/releases/latest/download/Vrutti_Setup.exe">
  <img src="https://img.shields.io/badge/Download-EXE_Installer-blue?style=for-the-badge&logo=windows&logoColor=white&scale=1.5" alt="Download EXE" height="40">
</a>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<a href="https://github.com/UnboundSB/Vrutti/releases/latest/download/vrutti_early_release.zip">
  <img src="https://img.shields.io/badge/Download-Portable_ZIP-ff69b4?style=for-the-badge&logo=files&logoColor=white&scale=1.5" alt="Download ZIP" height="40">
</a>

<br/>
<br/>

</div>

---

## ✨ Features That Wow

🚀 **Lightning Fast Engine**
<br/>
Robust C++ native backend communicating with a nimble Node.js/Lit frontend via IPC, keeping memory footprint low and UI responsiveness incredibly high.

🧬 **Topological Git Visualizer**
<br/>
Explore your commit history with a buttery-smooth, hardware-accelerated horizontal SVG canvas. Features drag-to-pan, interactive zooming, dynamic routing, reflog parsing, and direct git actions (Revert, Delete, Rename) right from the UI!

🎨 **Sleek & Premium Interface**
<br/>
Built with **Lit Web Components**, providing a modern, fast, and highly customizable user interface featuring beautiful pastel colors, sleek dark modes, and dynamic micro-animations that feel premium to the touch.

🛠 **Integrated Power Tools**
<br/>
Built-in task runner supporting custom `tasks.json` configuration natively tied to the embedded terminal, plus out-of-the-box syntax highlighting powered by CodeMirror.

---

## 🛠 For Developers (Building from Source)

**Only follow these steps if you want to modify Vrutti's source code.**

### Building the Core
To build the IDE locally:
1. Ensure you have **Docker**, **CMake**, and a **C++ compiler** installed.
2. Clone the repository: `git clone https://github.com/UnboundSB/Vrutti.git`
3. Build the frontend via Docker: `docker compose -f docker-compose.frontend.yml up`
4. Build the backend C++ application using CMake.
5. Run the generated `build/vrutti_app.exe`!

### Building the Release & Setup Installer
Want to package the `.zip` or installer yourself?
1. Ensure you have [Inno Setup 6](https://jrsoftware.org/isdl.php) installed on your Windows machine for EXE packaging.
2. Build the C++ backend and the frontend via Docker.
3. Run the packaging scripts:
   ```powershell
   # To build the portable ZIP:
   .\package_release.ps1

   # To build the EXE Installer (requires Inno Setup):
   .\build_installer.ps1
   ```
4. The scripts will generate `vrutti_early_release.zip` and `Vrutti_Setup.exe` in the root folder.

---

## 📜 License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

<div align="center">
  <br/>
  <h3>Built with ❤️ and ☕ for modern developers.</h3>
</div>
