# Vrutti IDE — src/core/updates/ TODO

**Scope:** `vrutti_ide/src/core/updates/` only. Parent `vrutti/` never touched.

## Purpose
Auto-update service: check for updates, download, and integrate with installer (`installer/`).

## Planned Files
| File | Purpose |
|------|---------|
| `UpdateService.h/.cpp` | Update check / download |
| `UpdateManager.h/.cpp` | Update orchestration |

## Tasks (Priority Order)
- [ ] **UPD1.** Implement update check against GitHub Releases API.
- [ ] **UPD2.** Download update package (portable ZIP or installer).
- [ ] **UPD3.** Verify checksum/signature of downloaded package.
- [ ] **UPD4.** Integrate with `build_installer.ps1` / `package_release.ps1` for installation.
- [ ] **UPD5.** Add `help.checkForUpdates` command.
- [ ] **UPD6.** Add auto-update toggle in settings (`update.enable`).

## Related
- `installer/TODO.md` — packaging
- `scripts/TODO.md` — release scripts
