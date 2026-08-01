# Vrutti IDE

Vrutti IDE is an open-source, extensible development environment built from the ground up for high performance and premium visual aesthetics.

## Features
- **Topological Git Visualizer**: Explore your commit history with a buttery-smooth, hardware-accelerated horizontal SVG canvas. Features drag-to-pan, interactive zooming, dynamic routing, reflog parsing, and direct git actions (Revert, Delete, Rename) right from the UI!
- **Sleek Interface**: Built with Lit Web Components, providing a modern, fast, and highly customizable user interface featuring pastel colors and sleek dark mode.
- **Fast Architecture**: Robust C++ native backend communicating with a nimble Node.js/Lit frontend via IPC.

## Downloading & Running
Currently, Vrutti is distributed via source. To build the IDE locally:
1. Ensure you have Docker, CMake, and a C++ compiler installed.
2. Clone the repository: `git clone https://github.com/UnboundSB/Vrutti.git`
3. Build the frontend via Docker: `docker compose -f docker-compose.frontend.yml up`
4. Build the backend C++ application using CMake.
5. Run the generated `vrutti_app.exe`!

## License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
