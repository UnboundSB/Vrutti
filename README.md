<div align="center">
<img src="https://raw.githubusercontent.com/UnboundSB/Vrutti/main/logos/logo-512x512.png" width="100" alt="Vrutti Logo">

# Vrutti IDE

A next-generation, high-performance development environment built from the ground up.

`Status: Active` &nbsp;·&nbsp; `License: MIT` &nbsp;·&nbsp; `Platform: Windows` &nbsp;·&nbsp; `Engine: C++`

</div>

<br>

## Downloads

<div align="center">

| | Installer | Portable |
|---|:---:|:---:|
| **File** | `Vrutti_Alpha_0.0.1_SleepyRabbit_Setup.exe` | `vrutti_early_release.zip` |
| **Best for** | Most users | No-admin / USB use |
| **Setup** | One-click, auto shortcuts, auto-update | Extract and run |
| **Requires** | Windows 10/11, admin rights | Windows 10/11 |
| **Get it** | [Download →](https://github.com/UnboundSB/Vrutti/releases/latest/download/Vrutti_Alpha_0.0.1_SleepyRabbit_Setup.exe) | [Download →](https://github.com/UnboundSB/Vrutti/releases/latest/download/vrutti_early_release.zip) |

</div>

<sub>Pre-built and ready to run — no Docker, CMake, or compiler required. Older builds live on the [Releases page](https://github.com/UnboundSB/Vrutti/releases).</sub>

<br>

## Overview

<table>
<tr><td width="33%" valign="top">

**Architecture**

Backend&nbsp;&nbsp;&nbsp;C++ (native)
Frontend&nbsp;&nbsp;&nbsp;Node.js + Lit
Bridge&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;IPC

</td><td width="33%" valign="top">

**Editor**

Highlighting&nbsp;&nbsp;CodeMirror
Tasks&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;`tasks.json`
Themes&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Dark / Pastel

</td><td width="33%" valign="top">

**Project**

Git&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Built-in visualizer
License&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;MIT
OS&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Windows 10/11

</td></tr>
</table>

<br>

## Features

| | Feature | Details |
|:---:|---|---|
| ⚡ | **Lightning Fast Engine** | Native C++ backend talks to the Lit/Node.js frontend over IPC — low memory footprint, high UI responsiveness. |
| 🧬 | **Topological Git Visualizer** | Hardware-accelerated horizontal SVG canvas with drag-to-pan, interactive zoom, dynamic routing, reflog parsing, and direct git actions (revert, delete, rename). |
| 🎨 | **Sleek Interface** | Lit Web Components, dark and pastel themes, subtle micro-animations throughout. |
| 🛠 | **Integrated Task Runner** | Native `tasks.json` support wired directly into the embedded terminal. |
| ✍️ | **Syntax Highlighting** | Powered by CodeMirror out of the box. |

<br>

## Quick Start

<table>
<tr><td width="8%" align="center"><b>1</b></td><td>Download the installer or ZIP from the table above</td></tr>
<tr><td align="center"><b>2</b></td><td>Run the installer, or extract the ZIP anywhere</td></tr>
<tr><td align="center"><b>3</b></td><td>Launch <code>vrutti_app.exe</code></td></tr>
<tr><td align="center"><b>4</b></td><td>Open a project folder — no configuration required</td></tr>
</table>

<br>

## Building From Source

<sub>Only needed if you're modifying Vrutti itself — everyone else should use the downloads above.</sub>

<table>
<tr><th width="50%" align="left">Build the core</th><th width="50%" align="left">Package a release</th></tr>
<tr valign="top"><td>

1. Install Docker, CMake, and a C++ compiler
2. `git clone https://github.com/UnboundSB/Vrutti.git`
3. `docker compose -f docker-compose.frontend.yml up`
4. Build the backend with CMake
5. Run `build/vrutti_app.exe`

</td><td>

1. Install [Inno Setup 6](https://jrsoftware.org/isdl.php)
2. Build the backend and frontend (steps at left)
3. `.\package_release.ps1` → portable ZIP
4. `.\build_installer.ps1` → `Vrutti_Alpha_0.0.1_SleepyRabbit_Setup.exe`

</td></tr>
</table>

<br>

## Contributing

<div align="center">

| Fork | Build | Submit | Get Credited |
|:---:|:---:|:---:|:---:|
| [Fork the repo](https://github.com/UnboundSB/Vrutti/fork) | Build & test locally | [Open a PR](https://github.com/UnboundSB/Vrutti/pulls) | Added to the contributors list |

</div>

Found a bug? [Open an issue](https://github.com/UnboundSB/Vrutti/issues) — contributions of any size are welcome.

<br>

## License

Licensed under the MIT License — see [LICENSE](LICENSE) for full terms.

<div align="center">
<sub>Built for developers who want a fast, modern editor without the setup overhead.</sub>
</div>
