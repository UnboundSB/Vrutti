# Vrutti IDE — src/ext/test_exts/ TODO

**Scope:** `vrutti_ide/src/ext/test_exts/` only. Parent `vrutti/` never touched.

## Purpose
Test extensions used to verify extension host compatibility (gitlens, prettier-vscode, vscode-eslint).

## Files
| Extension | Purpose | Status |
|-----------|---------|--------|
| `gitlens/` | Git history/blame extension | Test |
| `prettier-vscode/` | Code formatter | Test |
| `vscode-eslint/` | ESLint linter | Test |

## Tasks (Priority Order)
- [ ] **TEX1.** Verify each test extension loads without crashing the ExtHost.
- [ ] **TEX2.** Verify extension contributions (views, commands) register in frontend `Registry.ts`.
- [ ] **TEX3.** Record which `vscode` APIs each test extension uses; prioritize shimming those in `src/ext/api/`.
- [ ] **TEX4.** Add more test extensions (debugger, language server, theme) for broader coverage.

## Related
- `src/ext/api/TODO.md` — API shim layer informed by test extension usage
- `src/ext/protocols/TODO.md` — protocol methods needed by test extensions
