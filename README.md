# Vrutti

Vrutti is a high-performance, resource-efficient code editor designed for maximum speed and minimal system impact. Built entirely around a native C++ core with an embedded JavaScript engine, Vrutti offers a modern, extensible development environment without the heavy memory overhead typically associated with modern editors.

## Key Features

* **Extremely Low RAM Consumption:** Architected from the ground up to keep memory usage minimal, allowing developers to run heavy compilation tasks and background agents without the editor starving system resources.
* **Native C++ Core:** The engine relies on C++ for raw processing power, utilizing a Piece Table (a highly optimized data structure that tracks document edits efficiently) to open and edit massive files instantly.
* **Zero-Copy Extension Bridge:** Extensions are supported via a deeply integrated, lightweight JavaScript environment. By utilizing shared memory (a method where multiple systems can access the exact same block of computer RAM), Vrutti bypasses standard serialization delays, creating a frictionless and high-speed extension ecosystem.
* **Asynchronous Architecture:** External agents, language servers, and network requests are handled strictly on background multiplexing threads, guaranteeing that the primary typing interface never stutters or freezes.
* **Hybrid Native UI Compositor:** A custom C++ Webview wrapper that completely bypasses heavy frameworks (like Electron or Tauri), natively rendering a blazing-fast **Lit Web Components** frontend while consuming only ~350MB of RAM total.
* **Dynamic Plugin Architecture:** Complex features like Search are encapsulated into dynamically loaded native DLLs, lazy-loading only when requested and instantly freeing memory when closed.

## Getting Started

To build and run Vrutti, you need a C++20 compatible compiler (CMake) for the native core, and Node.js for the Lit frontend.

### 1. Running Using Self System (Local Node.js)
1. Install MSYS2 and open the UCRT64 terminal.
2. Install the toolchain:
   ```bash
   pacman -S --noconfirm mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-gcc
   ```
3. Build the native app:
   ```bash
   cmake -B build
   cmake --build build
   ```
4. Build the frontend (requires Node.js):
   ```bash
   cd src/ui/frontend
   npm install
   npm run build
   cd ../../../
   ```
5. Run the executable from `build/vrutti_app.exe`.

### 2. Running Using Docker (Isolated Frontend Build)
If you do not want to install Node.js locally, you can use Docker to compile the frontend:
1. Compile the native app using CMake (Steps 1-3 above).
2. Start the Docker frontend compiler:
   ```bash
   docker-compose -f docker-compose.frontend.yml up -d
   ```
3. Run the executable from `build/vrutti_app.exe`.

## Testing

Vrutti utilizes a rigorous two-pronged testing approach to guarantee zero-bloat and extreme performance:
1. **Native C++ Mocking (`ctest`)**: Core structures (Arena Allocators, File Watchers) and IPC channels are tested instantly using highly-optimized, dependency-free C++ mocks via `tests/TestFramework.h`.
2. **Dockerized Node.js Integration**: To test the JSON-RPC Extension Host Bridge authentically without polluting your local Windows environment, Vrutti spins up a lightweight Linux Docker container.
   ```bash
   docker compose -f docker-compose.test.yml run --build test-suite
   ```
