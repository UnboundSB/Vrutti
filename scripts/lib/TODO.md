# Vrutti IDE — scripts/lib/ TODO

**Scope:** `vrutti_ide/scripts/lib/` only. Parent `vrutti/` never touched.

## Purpose
Helpers shared by the scripts: the test harness, the init data and the tracing
preload.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `exthost_harness.js` | Starts the extension host and captures the frames it sends | Present |
| `init_data.js` | Builds the extension host init payload used by the tests | Present |
| `trace_preload.js` | Preloaded module that traces the calls of the extension host | Present |

## Tasks (Priority Order)
- [ ] **SL1.** Share the init data builder with the native service so both sides stay in sync.
- [ ] **SL2.** Let the harness assert on the frames instead of only printing them.
- [ ] **SL3.** Move the shared helpers into a small package with its own tests.

## Related
- `scripts/test_exthost_activation.js` — uses the harness.
- `scripts/test_exthost_handshake.js` — uses the harness.
- `src/ext/exthost/extensionHostProcess.ts` — the entry point under test.
