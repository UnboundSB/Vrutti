# Vrutti IDE Extension Host Bridge

This directory contains the Node.js components responsible for the Extension Host. 
To maintain the extreme performance and lightweight nature of Vrutti IDE, extensions do not run in the same process as the native C++ core. Instead, they run in a discrete, background Node.js process (the "Extension Host").

## Architecture

* **Bootstrapper (`bootstrapper.js`)**: The entry point for the Node.js process. It parses startup arguments from the C++ core (such as the IPC pipe name) and initializes the environment.
* **IPC Client (`ipc.js`)**: Handles the extremely low-latency communication between Node.js and the C++ engine using Windows named pipes. We use a lightweight JSON-RPC structure to pass messages back and forth asynchronously without blocking the UI.
* **Extension API (`api.js`)**: A highly optimized JavaScript polyfill (`vrutti`). Extensions code against this API as if they were directly manipulating the editor. Under the hood, this API simply serializes requests and sends them via the IPC client to the C++ core, which performs the actual heavy lifting on the piece tree or native UI.

By isolating extensions in this background process and relying purely on lightweight named pipes, the primary text editing experience in Vrutti IDE remains completely unblocked and stutter-free, no matter how much work an extension is doing.
