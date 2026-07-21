# Vrutti

Vrutti is a high-performance, resource-efficient code editor designed for maximum speed and minimal system impact. Built entirely around a native C++ core with an embedded JavaScript engine, Vrutti offers a modern, extensible development environment without the heavy memory overhead typically associated with modern editors.

## Key Features

* **Extremely Low RAM Consumption:** Architected from the ground up to keep memory usage minimal, allowing developers to run heavy compilation tasks and background agents without the editor starving system resources.
* **Native C++ Core:** The engine relies on C++ for raw processing power, utilizing a Piece Table (a highly optimized data structure that tracks document edits efficiently) to open and edit massive files instantly.
* **Zero-Copy Extension Bridge:** Extensions are supported via a deeply integrated, lightweight JavaScript environment. By utilizing shared memory (a method where multiple systems can access the exact same block of computer RAM), Vrutti bypasses standard serialization delays, creating a frictionless and high-speed extension ecosystem.
* **Asynchronous Architecture:** External agents, language servers, and network requests are handled strictly on background multiplexing threads, guaranteeing that the primary typing interface never stutters or freezes.
* **Native UI Compositor:** A highly optimized Dear ImGui frontend that bypasses the DOM entirely, rendering a professional DockSpace IDE layout natively on the GPU.
* **Dynamic Plugin Architecture:** Complex features like Search are encapsulated into dynamically loaded native DLLs, lazy-loading only when requested and instantly freeing memory when closed.

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

## Testing

Vrutti utilizes a rigorous two-pronged testing approach to guarantee zero-bloat and extreme performance:
1. **Native C++ Mocking (`ctest`)**: Core structures (Arena Allocators, File Watchers) and IPC channels are tested instantly using highly-optimized, dependency-free C++ mocks via `tests/TestFramework.h`.
2. **Dockerized Node.js Integration**: To test the JSON-RPC Extension Host Bridge authentically without polluting your local Windows environment, Vrutti spins up a lightweight Linux Docker container.
   ```bash
   docker compose -f docker-compose.test.yml run --build test-suite
   ```
