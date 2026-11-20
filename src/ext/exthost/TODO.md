# Vrutti IDE — src/ext/exthost/ TODO

**Scope:** `vrutti_ide/src/ext/exthost/` only. Parent `vrutti/` never touched.

## Purpose
The extension host entry point. Vrutti ships its own launcher for the shared
extension host implementation because the upstream bridge has a byte-collision
bug that prevents every extension from activating.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `extensionHostProcess.ts` | The entry point: connects to the renderer, sends the init data and starts the extension host main object | Present |

## Tasks (Priority Order)
- [ ] **EH1.** Bundle the entry point with esbuild so the release does not depend on `tsx`.
- [ ] **EH2.** Keep the error codes of this file in sync with `src/core/utils/ErrorCodes.h`.
- [ ] **EH3.** Add a handshake timeout so a silent host does not block the start-up forever.
- [ ] **EH4.** Cover the bridge with a unit test that replays a recorded frame stream.

## Related
- `src/ext/bootstrapper.js` — finds and starts this entry point.
- `src/ext/errorCodes.js` — the generated mirror of the error code registry.
- `src/core/ipc/VSCodeProtocol.cpp` — the other side of the bridge.
