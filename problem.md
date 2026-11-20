# Vrutti IDE - Identified Problems and Potential Solutions (Prioritized)

After rigorous architectural auditing, the following 27 issues have been cataloged across the codebase. They are sorted by priority, starting with the most critical security vulnerabilities, down to architectural schizophrenia and finally brittle logic.

## Priority 1: Critical Security Vulnerabilities (RCE / XSS)

### 1. Command Injection (RCE) via `_popen` in Extension Installer
- **Location:** `src/ui/compositor/Window.cpp` (Lines 556, 612)
- **Problem:** When installing or uninstalling extensions, the application concatenates untrusted JSON payload strings (`url` and `name`) directly into a shell command that is executed via `_popen()`. This exposes the application to severe Command Injection (RCE).
- **Solution:** Stop using shell execution. Replace the shell scripts with native C++ libraries for downloading (e.g., `libcurl`) and unpacking ZIP files (e.g., `libzip`).

### 2. Command Injection (RCE) in Git and Run Handlers
- **Location:** `src/ui/compositor/Window.cpp` (Lines 945, 948, 1014, 1017)
- **Problem:** In `vruttiGitCommand` and `vruttiRunFile`, user-provided strings such as `cwd` and `cmd` are blindly concatenated into shell strings: `std::string fullCmd = "cd /d \"" + cwd + "\" && " + cmd + " 2>&1";`. Any input containing a command separator like `&&` or `;` will execute arbitrary commands on the host machine.
- **Solution:** Use `CreateProcess` (Windows) or `execvp` (POSIX) to spawn processes securely, passing arguments in arrays rather than concatenating them into shell strings.

### 3. Webview Script Injection (XSS) via `eval`
- **Location:** `src/ui/compositor/Window.cpp` (Lines 240, 579)
- **Problem:** IPC handlers take parameters from the Webview and concatenate them directly into Javascript string literals evaluated via `w->eval(...)`. An attacker who controls the parameters can break out of the string literal with a single quote (`'`) and inject arbitrary JS code into the privileged Webview context.
- **Solution:** Escape all parameters using `vrutti::core::utils::JsonSerializer::escapeString` before appending them to the string.

### 4. Insecure Extension DOM Injection (XSS)
- **Location:** `src/ui/frontend/src/main.ts` (Line 121, 125)
- **Problem:** The IPC method `extensions/injections` takes raw CSS and JS provided by an extension and injects it directly into the IDE's main DOM using `style.innerHTML` and `script.innerHTML`. A malicious extension has full XSS access to the entire frontend UI.
- **Solution:** Implement a strict sandboxed iframe layer for extensions and use postMessage instead of direct DOM injections.

### 5. Unsafe HTML Rendering in Lit Components
- **Location:** `src/ui/frontend/src/components/scm/vrutti-scm.ts` (Lines 369, 402, 424) and others
- **Problem:** UI components use Lit's `unsafeHTML(item.iconContent)` to render icons dynamically provided by extensions. An extension can provide an SVG containing `<script>` tags, causing an immediate XSS execution.
- **Solution:** Sanitize all incoming strings using a DOM purifier (e.g., `DOMPurify`) before passing them to `unsafeHTML`.


## Priority 2: Severe Logical Flaws (Hangs / Deadlocks / Memory Leaks)

### 6. UI Thread Deadlock / Hanging on Shutdown
- **Location:** `src/core/terminal/TerminalProcess.cpp` (Line 155)
- **Problem:** `TerminalProcess::stop()` unconditionally calls `m_readThread.join()`. The read thread relies on a blocking `ReadFile` call. Even if `TerminateProcess` is called, if any spawned descendant inherited the pipe handle, `ReadFile` blocks indefinitely. This will completely freeze the IDE during shutdown.
- **Solution:** Issue a cancel I/O request (e.g. `CancelIoEx`) or asynchronously detach the read thread.

### 7. Thread Synchronization Blocking & Memory Leaks in Services
- **Location:** `src/core/plugins/TerminalService.cpp` and `DebugAdapterService.cpp`
- **Problem:** `shutdown()` acquires a lock and synchronously `join()`s a background thread that `sleep_for` 500ms, freezing the UI. Furthermore, inactive `PtySession` and `DebugSession` pointers are never removed from `m_sessions` during normal usage, leading to unbounded memory leaks.
- **Solution:** Use condition variables to safely wake threads. Remove deleted sessions from maps proactively.

### 8. Blocking Disk I/O within Async Promises
- **Location:** `src/core/config/SettingsManager.cpp` (Lines 104-106)
- **Problem:** The `updateSetting` method calls `saveAsync()`, then immediately calls `m_saveFuture.wait()` while holding `mutex_`. The caller is blocked on disk I/O, defeating the purpose of an asynchronous save and freezing the UI.
- **Solution:** Push write requests to a dedicated thread-safe queue consumed by a background worker.

### 9. Dangling IPC Promises & Memory Leak
- **Location:** `src/ui/compositor/Window.cpp` (Line 195) vs Frontend `sendIpcMessage`
- **Problem:** The frontend calls `sendIpcMessage` inside Promise wrappers expecting a resolution. However, the C++ binding uses the 3-argument signature `(seq, req, arg)` but **never calls the webview's internal promise resolver**. Every IPC call leaks a pending Promise that never resolves, slowly hanging components that use `await`.
- **Solution:** Explicitly call the webview's promise resolver in C++ after sending the IPC message.

### 10. Process Zombie Factory (Orphaned Child Processes)
- **Location:** `src/ext/legacy/bootstrapper.js` (and dap-client.js / lsp-client.js)
- **Problem:** When the C++ Core exits, `ipc.js` catches the pipe error and calls `process.exit(0)`. Because it abruptly exits without sending `kill()` to its children, all language servers and debuggers are orphaned as zombie processes forever.
- **Solution:** Hook `process.on('exit')` to actively track and kill all spawned child processes.

### 11. Hardcoded IPC Named Pipe Collisions
- **Location:** `src/app/main.cpp` (Line 38 & 62)
- **Problem:** The named pipe for the Node.js extension host is hardcoded to `vrutti_pipe`. Running multiple IDE instances simultaneously will result in catastrophic IPC collisions.
- **Solution:** Append the OS Process ID or a UUID to the pipe name (`vrutti_pipe_10520`).

### 12. PluginLoader Race Conditions
- **Location:** `src/core/plugins/PluginLoader.cpp`
- **Problem:** `PluginLoader` modifies `m_loadedPlugins` without any thread-safety guarantees. If plugins are dynamically loaded in background threads, it will crash.
- **Solution:** Add an `std::mutex` and use a `lock_guard` around all map accesses.

### 13. Lit Component Event Listener Leaks (`this` binding)
- **Location:** `src/ui/frontend/src/components/vrutti-explorer-view.ts` (Line 14)
- **Problem:** The frontend adds listeners via `window.addEventListener('event', this.handleEvent as unknown as EventListener)`. Because the handlers are not arrow functions, `this` evaluates to the `window` object when fired, breaking logic and leaking memory.
- **Solution:** Use arrow functions for all component event handlers.


## Priority 3: Architecture & Integration Schizophrenia

### 14. Schizophrenic Extension Manager Integration
- **Location:** `src/ui/compositor/Window.cpp` vs `bootstrapper.js`
- **Problem:** The AI implemented two totally redundant backend systems to manage extensions. The UI blindly calls C++ webview bindings like `window.vruttiInstallExtension` (which uses dangerous `_popen`), bypassing the perfectly functioning Node.js `ExtensionManager` in `bootstrapper.js`. The C++ and Node.js states get out of sync immediately.
- **Solution:** Remove the C++ shell scripts entirely and route all extension management over IPC to the Node.js extension host.

### 15. Split-Brain Debug Adapter Protocol (DAP) Implementations
- **Location:** `src/core/plugins/DebugAdapterService.cpp` vs `src/ext/legacy/bootstrapper.js`
- **Problem:** The frontend emits `dap/start` via IPC. `bootstrapper.js` launches a Node.js `DapClient`. However, the AI also built a completely separate, redundant C++ module `DebugAdapterService` sitting in `src/core/plugins/` doing exactly the same thing.
- **Solution:** Deprecate and delete the C++ DAP service; rely purely on the Extension Host.

### 16. Brittle String-Concatenated JSON IPC & Parsing Failures
- **Location:** `src/core/ipc/IPCClient.cpp` (Line 83) and `Window.cpp` (Line 195)
- **Problem:** C++ blindly concatenates JS string payloads to form JSON. In `Window.cpp`, the binding strictly enforces `payloadNode->type == String`, forcing the frontend to `JSON.stringify()` all payloads. Unquoted literal strings from the frontend create invalid JSON that silently crashes `JSON.parse` inside Node.js.
- **Solution:** Use a robust C++ JSON library to properly serialize payloads instead of string concatenation.

### 17. IPC JSON Parsing (String Matching)
- **Location:** `src/core/ipc/IPCClient.cpp` (Lines 125-131)
- **Problem:** The C++ `IPCClient` parses incoming JSON from Node.js using naive string matching (`jsonMessage.find("\"method\":\"insert\"")`). This is logically wrong and extremely fragile.
- **Solution:** Integrate `nlohmann/json` to safely deserialize incoming IPC payloads.


## Priority 4: Hardcoded Paths, Fragile Regex, and Brittle Logic

### 18. Hardcoded Cross-Platform Path Disasters
- **Location:** `src/app/main.cpp` (Line 62) and `src/ui/frontend/src/main.ts` (Line 98)
- **Problem:** The AI hardcoded Windows-specific absolute paths (`d:/vrutti/out/vs/workbench.desktop.main.css`) directly into the frontend. C++ also spawns the extension host using a rigid relative path (`src\\ext\\bin\\node.exe`). Launching the IDE from outside `d:/vrutti/` results in silent catastrophic failure.
- **Solution:** Resolve paths dynamically relative to the application's executable directory.

### 19. Hardcoded Open VSX Marketplace URL
- **Location:** `src/ui/frontend/src/components/vrutti-extensions.ts` (Line 281)
- **Problem:** The Open VSX API URL is hardcoded into the frontend search fetch call.
- **Solution:** Move the extension registry URL into the global application configuration.

### 20. Brittle Local Path Transformation (Hack)
- **Location:** `src/ui/frontend/src/components/vrutti-extensions.ts` (Line 218)
- **Problem:** Extension icon URLs fall back to a hardcoded string replacement hack: `` `file:///${(ext.localPath || '').replace(/\\/g, '/')}/${ext.icon}` ``.
- **Solution:** The Node.js backend should provide fully resolved URIs before sending them over IPC.

### 21. Hardcoded UI Colors
- **Location:** `src/ui/frontend/src/components/vrutti-extensions.ts` (Line 389)
- **Problem:** Buttons contain inline hardcoded color styles: `style="background: #e81123;"`.
- **Solution:** Migrate to global CSS variables defined in the theme system.

### 22. Legacy and Fragile UTF-8 Base64 Decoding
- **Location:** `src/ui/frontend/src/main.ts` (Line 110)
- **Problem:** `(window as any).vruttiIpcMessage` uses the deprecated trick `decodeURIComponent(escape(atob(b64)))` to parse base64 strings containing UTF-8 characters. This polyfill is known to corrupt complex multi-byte Unicode strings (e.g. emojis).
- **Solution:** Use the native `TextDecoder` API (`new TextDecoder().decode(...)`).

### 23. Fragile Regex for DAP Content-Length
- **Location:** `src/ext/dap-client.js` (Line 125)
- **Problem:** The regex `header.match(/Content-Length: (\d+)/i)` strictly expects exactly one space after the colon. If a DAP server uses multiple spaces, the connection breaks completely.
- **Solution:** Use `\s*` to allow flexible spacing.
