# Vrutti IDE — src/ext/bridge/ TODO

**Scope:** `vrutti_ide/src/ext/bridge/` only. Parent `vrutti/` never touched.

## Purpose
Node.js V8 bridge headers (node-v26.7.0). Used to embed Node/V8 into the Extension Host process. Third-party headers — do not modify.

## Status
- Third-party Node.js headers (read-only)
- No application code should live here

## Tasks (Priority Order)
- [ ] **BRG1.** Do not edit any files in this directory.
- [ ] **BRG2.** Verify header version matches the `node.exe` binary in `bin/`.
- [ ] **BRG3.** Document licensing (Node.js is MIT-licensed; V8 is BSD-style).

## Related
- `src/ext/bin/TODO.md` — node.exe binary
