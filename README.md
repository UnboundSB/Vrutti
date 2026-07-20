# Vrutti

Vrutti is a high-performance, resource-efficient code editor designed for maximum speed and minimal system impact. Built entirely around a native C++ core with an embedded JavaScript engine, Vrutti offers a modern, extensible development environment without the heavy memory overhead typically associated with modern editors.

## Key Features

* **Extremely Low RAM Consumption:** Architected from the ground up to keep memory usage minimal, allowing developers to run heavy compilation tasks and background agents without the editor starving system resources.
* **Native C++ Core:** The engine relies on C++ for raw processing power, utilizing a Piece Table (a highly optimized data structure that tracks document edits efficiently) to open and edit massive files instantly.
* **Zero-Copy Extension Bridge:** Extensions are supported via a deeply integrated, lightweight JavaScript environment. By utilizing shared memory (a method where multiple systems can access the exact same block of computer RAM), Vrutti bypasses standard serialization delays, creating a frictionless and high-speed extension ecosystem.
* **Asynchronous Architecture:** External agents, language servers, and network requests are handled strictly on background multiplexing threads, guaranteeing that the primary typing interface never stutters or freezes.

## Getting Started

To build Vrutti, you need a C++20 compatible compiler and CMake. We recommend using MSYS2 on Windows.

1. Install MSYS2.
2. Open the MSYS2 UCRT64 terminal and install the toolchain:
   ```bash
   pacman -S --noconfirm mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-gcc
   ```
3. Clone the repository and run CMake:
   ```bash
   cmake -B build
   cmake --build build
   ```
4. Run the executable from `build/vrutti_app.exe`.
