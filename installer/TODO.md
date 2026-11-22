# Vrutti IDE — installer/ TODO

**Scope:** `vrutti_ide/installer/` only. Parent `vrutti/` never touched.

## Purpose
Installer and packaging scripts: Inno Setup 6 (`.exe` installer) and PowerShell (portable ZIP).

## Files
| File | Purpose | Status |
|------|---------|--------|
| `build_installer.ps1` | Inno Setup installer build | Present |
| `package_release.ps1` | Portable ZIP packaging | Present |
| `test_install.ps1` | Installer testing | Present |

## Tasks (Priority Order)
- [x] **INS1.** Verify `build_installer.ps1` includes `vrutti.exe`, all plugin DLLs, frontend `dist/`, `node.exe`, the extension host bundle and `rg.exe`.
- [x] **INS2.** Confirm Inno Setup 6 (`ISCC.exe`) is installed and `installer\VruttiSetup.iss` compiles to `Vrutti_Alpha_0.0.4_SleepyRabbit_Setup.exe`.
- [x] **INS3.** Verify `package_release.ps1` creates a working portable ZIP.
- [ ] **INS4.** Add installer uninstaller that cleans up `.vrutti` data folder (optional, user-configurable).
- [ ] **INS5.** Add code signing for the installer executable.
- [ ] **INS6.** Test installer on clean Windows VM (no dev tools installed).

## Build Pipeline (Distribution)
```
CMake/Makefile  →  build\vrutti.exe + *.dll
Vite            →  src\ui\frontend\dist
build_installer.ps1 → copies all into Vrutti_Release/
Inno Setup 6    →  installer\VruttiSetup.iss → Vrutti_Alpha_0.0.4_SleepyRabbit_Setup.exe
```

## Related
- `src/core/updates/TODO.md` — auto-update downloads installer/ZIP
- `scripts/TODO.md` — release helper scripts
- `bin/TODO.md` — ripgrep binary inclusion


## Related
- `src/core/updates/TODO.md` — auto-update downloads installer/ZIP
- `scripts/TODO.md` — release helper scripts
- `bin/TODO.md` — ripgrep binary inclusion
