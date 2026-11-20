# Vrutti IDE — src/ui/frontend/public/ TODO

**Scope:** `vrutti_ide/src/ui/frontend/public/` only. Parent `vrutti/` never touched.

## Purpose
Static assets that are copied into the renderer bundle as they are: icons,
fonts and the files the workbench loads at runtime.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `*.svg`, `*.png` | Icons and images used by the renderer | Present |

## Tasks (Priority Order)
- [ ] **PU1.** Move the product icons here so the renderer and the installer use the same files.
- [ ] **PU2.** Check that every asset is covered by the release packaging.
- [ ] **PU3.** Remove the assets that no component references.

## Related
- `src/ui/frontend/src/components/codicons.ts` — the icon set used by the workbench.
- `build_installer.ps1` — copies the renderer into the installation.
