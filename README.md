# Vrutti

Vrutti is a high-performance, resource-efficient code editor designed for maximum speed and minimal system impact. Built entirely around a native C++ core with an embedded JavaScript engine, Vrutti offers a modern, extensible development environment without the heavy memory overhead typically associated with modern editors.

## Key Features

* **Extremely Low RAM Consumption:** Architected from the ground up to keep memory usage minimal, allowing developers to run heavy compilation tasks and background agents without the editor starving system resources.
* **Native C++ Core:** The engine relies on C++ for raw processing power, utilizing a Piece Table (a highly optimized data structure that tracks document edits efficiently) to open and edit massive files instantly.
* **Zero-Copy Extension Bridge:** Extensions are supported via a deeply integrated, lightweight JavaScript environment. By utilizing shared memory (a method where multiple systems can access the exact same block of computer RAM), Vrutti bypasses standard serialization delays, creating a frictionless and high-speed extension ecosystem.
* **Asynchronous Architecture:** External agents, language servers, and network requests are handled strictly on background multiplexing threads, guaranteeing that the primary typing interface never stutters or freezes.

## Getting Started

*(Build instructions and configuration steps will be updated as the core engine development progresses.)*
