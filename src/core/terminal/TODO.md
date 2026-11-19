# Vrutti IDE — src/core/terminal/ TODO

**Scope:** `vrutti_ide/src/core/terminal/` only. Parent `vrutti/` never touched.

## Purpose
Native PTY (Pseudo-Terminal) broker for the integrated terminal. Uses ConPTY on Windows, forkpty on POSIX. Streams raw stdout/stderr bytes to the frontend via IPC.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `TerminalProcess.cpp/.h` | PTY process lifecycle | Stub (mock reader exits after 500ms) |
| `ReplProcess.cpp/.h` | REPL process handling | Present |

## Critical Bugs (from problem.md)
- [ ] **T1.** `TerminalProcess::stop()` joins a thread blocked on `ReadFile` → shutdown deadlock. Fix: use `CancelIoEx` or async detach.
- [ ] **T2.** Inactive `PtySession` pointers never removed from `m_sessions` → unbounded memory leak. Fix: remove dead sessions proactively.
- [ ] **T3.** Reader thread uses `sleep(500)` mock instead of real PTY read. Fix: blocking read on PTY stdout pipe.

## Remaining Tasks (Priority Order)
- [ ] **T4.** Implement real `spawnPty()`: `CreatePseudoConsole` (Windows) or `forkpty` (POSIX).
- [ ] **T5.** Implement `handleTerminalWrite()`: write bytes to PTY stdin handle.
- [ ] **T6.** Implement `handleTerminalResize()`: `ResizePseudoConsole` / `ioctl TIOCSWINSZ`.
- [ ] **T7.** Stream PTY output to frontend via IPC notification `terminal/data` (not console.log).
- [ ] **T8.** Support multiple terminal sessions with unique IDs and tab management.
- [ ] **T9.** Shell environment variables and working directory support.
- [ ] **T10.** Graceful terminal shutdown: kill process tree, close PTY handles, join reader thread.

## Related
- `src/core/plugins/TerminalService.cpp` — wraps this into an IPlugin
- `src/ui/frontend/src/components/vrutti-terminal.ts` — xterm.js frontend consumer
- `src/core/ipc/TODO.md` — IPC `terminal/*` methods
