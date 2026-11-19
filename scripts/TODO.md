# Vrutti IDE — scripts/ TODO

**Scope:** `vrutti_ide/scripts/` only. Parent `vrutti/` never touched.

## Purpose
Build and release helper scripts. (NEW directory — scripts moved here from root for organization.)

## Planned Files
| File | Purpose |
|------|---------|
| `build-core.ps1` | Build C++ core with CMake |
| `build-frontend.ps1` | Build Lit frontend with Vite |
| `release.ps1` | Full release pipeline (core + frontend + package) |
| `version.ps1` | Version bump and git tag |
| `clean.ps1` | Clean build artifacts |

## Tasks (Priority Order)
- [ ] **SCR1.** Move existing root scripts (`build_installer.ps1`, `package_release.ps1`, `test_install.ps1`, `gen_docs.ps1`, `update_structures.js`) into this directory.
- [ ] **SCR2.** Create `build-core.ps1` — CMake build of all plugins + app → `build\vrutti_app.exe`.
- [ ] **SCR3.** Create `build-frontend.ps1` — `npm install` + `npm run build` in `src/ui/frontend/` → `dist/`.
- [ ] **SCR4.** Create `release.ps1` — orchestrates build-core → build-frontend → `build_installer.ps1` → Inno Setup 6 → `Vrutti_Setup.exe`.
- [ ] **SCR5.** Add script to verify no hardcoded paths in source (security/path audit).

## Build Pipeline (Distribution)
```
Step 1: CMake/Makefile        → build\vrutti_app.exe + plugin DLLs
Step 2: Vite                  → src\ui\frontend\dist
Step 3: build_installer.ps1   → assembles Vrutti_Release/ (exe + DLLs + dist + node.exe + themes)
Step 4: Inno Setup 6 (ISCC)   → installer\VruttiSetup.iss → Vrutti_Setup.exe
```

## Related
- `installer/TODO.md` — packaging
- `docs/TODO.md` — documentation generation


## Related
- `installer/TODO.md` — packaging
- `docs/TODO.md` — documentation generation
