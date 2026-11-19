# Vrutti IDE — src/ext/legacy/ TODO

**Scope:** `vrutti_ide/src/ext/legacy/` only. Parent `vrutti/` never touched.

## Purpose
Legacy bootstrapper and API shim files kept for reference/migration. Not part of the active extension host path.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `bootstrapper.js` | Legacy bootstrapper | Deprecated |
| `api.js` | Legacy API shim | Deprecated |

## Tasks (Priority Order)
- [ ] **LEG1.** Mark all files as deprecated with clear headers.
- [ ] **LEG2.** Ensure no active code imports from `legacy/`.
- [ ] **LEG3.** Remove `legacy/` once migration to `bootstrapper.js` + `api/` is complete.

## Related
- `src/ext/bootstrapper.js` — active bootstrapper
- `src/ext/api/TODO.md` — replacement API shim
