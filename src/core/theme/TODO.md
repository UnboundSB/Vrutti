# Vrutti IDE — src/core/theme/ TODO

**Scope:** `vrutti_ide/src/core/theme/` only. Parent `vrutti/` never touched.

## Purpose
Theme service: reads VS Code color theme files, maps them to CSS variables, pushes to frontend via IPC.

## Planned Files
| File | Purpose |
|------|---------|
| `ThemeService.h/.cpp` | Load / apply color themes |
| `ThemeResolver.h/.cpp` | Map VS Code theme → CSS variables |

## Tasks (Priority Order)
- [ ] **THM1.** Implement theme loading from `.json` theme files (built-in + extension-contributed).
- [ ] **THM2.** Map VS Code token colors to CSS custom properties (`--vrutti-*`).
- [ ] **THM3.** Apply theme to frontend via IPC `theme/set`.
- [ ] **THM4.** Support `workbench.colorTheme` setting changes at runtime.
- [ ] **THM5.** Support icon themes (`workbench.iconTheme`).
- [ ] **THM6.** Integrate with `ThemeBridge.ts` in frontend.

## Related
- `src/ui/frontend/src/ThemeBridge.ts` — frontend theme bridge
- `src/ui/frontend/src/shared-styles.ts` — CSS custom properties
- `src/core/configuration/TODO.md` — `workbench.colorTheme` setting
