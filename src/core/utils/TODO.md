# Vrutti IDE — src/core/utils/ TODO

**Scope:** `vrutti_ide/src/core/utils/` only. Parent `vrutti/` never touched.

## Purpose
Shared helper classes: JSON parsing/serialization, Base64 encoding, line scanning, string pooling.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `Json.cpp/.h` | JSON parser | Present |
| `Base64.cpp/.h` | Base64 encode/decode | Present |
| `LineScanner.cpp/.h` | Line scanning | Present |
| `StringPool.cpp/.h` | String interning pool | Present |

## Tasks (Priority Order)
- [ ] **UT1.** Consider replacing custom `Json.h` with `nlohmann::json` for consistency with `IPCClient` (already using nlohmann).
- [ ] **UT2.** Add `escapeString` for safe JSON string embedding (used by IPC).
- [ ] **UT3.** Add UTF-8 safe Base64 decode (replace `decodeURIComponent(escape(atob()))` polyfill in frontend).
- [ ] **UT4.** Add `StringPool` deduplication for frequently-repeated strings (extension IDs, URIs).

## Related
- `src/core/ipc/TODO.md` — uses Json for serialization
- `src/ui/frontend/src/main.ts` — Base64 UTF-8 fix needed here
