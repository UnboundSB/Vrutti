# Vrutti IDE — Root TODO

**Scope:** `vrutti_ide/` only. Parent `vrutti/` (upstream VS Code source) is never touched.

## Purpose
Root of the Vrutti IDE workspace. Contains build configuration, release packaging, product config, and architectural docs.

## Directory Inventory
| Dir | Purpose |
|-----|---------|
| `src/` | All source code (C++ core, Node ext host, Lit frontend) |
| `bin/` | Ripgrep binary and other native tools |
| `logos/` | Branding assets |
| `installer/` | Inno Setup / PowerShell packaging scripts |
| `scripts/` | Build and release helper scripts (NEW) |
| `docs/` | Architectural documentation (NEW) |
| `.agents/` | Agent/organizational config |

## Root-Level Tasks (Priority Order)
- [ ] **R1.** Consolidate scattered architecture docs (`architecture.md`, `SYSTEM_ARCHITECTURE.md`, `problem.md`, `task.md`, `tasks.md`, `summary.md`, `structure.md`) into a single source of truth under `docs/`.
- [ ] **R2.** Move build/release scripts (`build_installer.ps1`, `package_release.ps1`, `test_install.ps1`, `gen_docs.ps1`, `update_structures.js`) into `scripts/`.
- [ ] **R3.** Verify `product.json` has all required fields (nameShort, nameLong, applicationName, dataFolderName, win32MutexName, urlProtocol, extensionAllowedProposedApi).
- [ ] **R4.** Ensure `.gitignore` excludes `build/`, `dist/`, `dist_new/`, `node_modules/`, `.git/`, `*.exe`, `*.zip`, `*.vsix`.
- [ ] **R5.** Confirm `CMakeLists.txt` builds all 16 plugin DLLs + `vrutti_core` static lib + `vrutti_app.exe`.
- [ ] **R6.** Confirm `package.json` at root (if any) or `src/ui/frontend/package.json` builds the Lit frontend via Vite.
- [ ] **R7.** Confirm `build_installer.ps1` + Inno Setup 6 (`ISCC.exe`) produces `Vrutti_Setup.exe` from `Vrutti_Release/`.

## Cross-Cutting Concerns
- Security: eliminate `_popen`, `eval`, direct DOM `innerHTML` injection (see `problem.md`).
- Stability: fix terminal shutdown deadlock, IPC promise leaks, session memory leaks, `SettingsManager` blocking wait.
- Paths: zero hardcoding — resolve everything from executable directory.

## Related
- `src/TODO.md` — source tree breakdown
- `architecture.md` — full VS Code architecture comparison (Mermaid diagram)
- `tasks.md` — 10-release backend→frontend roadmap
