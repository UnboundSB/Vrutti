# Vrutti IDE — src/tests/integration/ TODO

**Scope:** `vrutti_ide/src/tests/integration/` only. Parent `vrutti/` never touched.

## Purpose
End-to-end integration tests that launch the full Vrutti IDE and verify workflows.

## Tasks (Priority Order)
- [ ] **INT1.** Test harness: script to launch `vrutti_app.exe` with a test workspace.
- [ ] **INT2.** E2E: open folder → verify workspace folders in UI.
- [ ] **INT3.** E2E: open file → edit → save → verify file on disk changed.
- [ ] **INT4.** E2E: open terminal → run command → verify output appears.
- [ ] **INT5.** E2E: install extension → verify it appears in extensions sidebar.
- [ ] **INT6.** E2E: close app → verify no orphaned processes, no hangs.
- [ ] **INT7.** E2E: launch from arbitrary directory (not `d:/vrutti/`) → verify paths resolve.

## Related
- `src/tests/TODO.md` — overall test strategy
- `src/app/TODO.md` — app launch for tests
