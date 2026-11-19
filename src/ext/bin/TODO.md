# Vrutti IDE — src/ext/bin/ TODO

**Scope:** `vrutti_ide/src/ext/bin/` only. Parent `vrutti/` never touched.

## Purpose
Node.js binary and helper binaries for the Extension Host process.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `node.exe` | Node.js runtime for ExtHost | Present |

## Tasks (Priority Order)
- [ ] **BIN1.** Document the exact Node.js version required (v26.7.0 headers present in bridge/).
- [ ] **BIN2.** Ensure `node.exe` is included in release builds (portable ZIP and installer).
- [ ] **BIN3.** Add version check: refuse to start if Node version mismatches.

## Related
- `src/app/main.cpp` — spawns `node.exe`
- `src/ext/bootstrapper.js` — runs under node.exe
