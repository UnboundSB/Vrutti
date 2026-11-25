# Vrutti IDE — src/tests/integration/ TODO

**Scope:** `vrutti_ide/src/tests/integration/` only. Parent `vrutti/` never touched.

## Purpose
End-to-end integration tests that launch the full Vrutti IDE and verify workflows.

## Tasks (Priority Order)
- [x] **INT0.** The contract of the extension host router: a call without an
      implementation must be answered with the benign default of its return
      type, never with an error. (`ProtocolFallbackTest.cpp`, driven over the
      real named pipe.)
- [x] **INT0b.** The `workspace.fs` surface: an extension writes a file,
      reads it back, stats it, lists a directory, creates a folder, renames,
      copies, deletes, and gets the upstream error names for the failures.
      (`ExthostFileSystemTest.cpp`, over the real named pipe.)
- [ ] **INT1.** Test harness: script to launch `vrutti_app.exe` with a test workspace.
- [ ] **INT2.** E2E: open folder → verify workspace folders in UI.
- [ ] **INT3.** E2E: open file → edit → save → verify file on disk changed.
- [ ] **INT4.** E2E: open terminal → run command → verify output appears.
- [ ] **INT5.** E2E: install extension → verify it appears in extensions sidebar.
- [ ] **INT6.** E2E: close app → verify no orphaned processes, no hangs.
- [ ] **INT7.** E2E: launch from arbitrary directory (not `d:/vrutti/`) → verify paths resolve.
- [x] **INT8.** Fuzz the JSON parser with random and truncated input.
      (`JsonFuzzTest.cpp` — random text, text that is cut in half, text that
      is nested very deeply, and a serializer that is given numbers that do
      not exist.)
- [x] **INT9.** Fuzz the IPC frame decoder: a frame that claims a length it
      does not have must be dropped, not crash the core. (`IpcFuzzTest.cpp` —
      frames that are not JSON, frames that never end, frames that are too
      big, and calls that are never answered.)
- [x] **INT10.** A request that is never answered must fail on its deadline
      instead of hanging forever. (`IpcFuzzTest.cpp` — the call is given up
      on, and the table of the open calls stays bounded.)
- [x] **INT11.** The parts of the core that hold data, under hostile input.
      (`CoreHardeningTest.cpp` — the binary payloads, the paths of the
      installation, the version range of an extension, the entry point of an
      extension, the files of the user, the text of a document, the settings
      and the themes.)

## Related
- `src/tests/TODO.md` — overall test strategy
- `src/app/TODO.md` — app launch for tests
- `ExtHostPipeClient.h` — the pipe client the integration tests share
