# Vrutti IDE — src/ext/builtin-themes/ TODO

**Scope:** `vrutti_ide/src/ext/builtin-themes/` only. Parent `vrutti/` never touched.

## Purpose
Default bundled VS Code-compatible themes (syntax highlighting + UI colors).

## Tasks (Priority Order)
- [ ] **THM1.** Add `vrutti-default-dark` theme (dark mode).
- [ ] **THM2.** Add `vrutti-default-light` theme (light mode).
- [ ] **THM3.** Add `vrutti-pastel` theme (existing pastel theme).
- [ ] **THM4.** Ensure themes follow VS Code `.json` theme format for extension compatibility.
- [ ] **THM5.** Wire to `core/theme/TODO.md` for application at runtime.

## Related
- `src/core/theme/TODO.md` — theme service
- `src/ui/frontend/src/ThemeBridge.ts` — frontend theme application
