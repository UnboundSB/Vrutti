# Vrutti IDE — src/core/telemetry/ TODO

**Scope:** `vrutti_ide/src/core/telemetry/` only. Parent `vrutti/` never touched.

## Purpose
Lightweight telemetry pipeline: opt-in event collection, no PII, crash reporting integration.

## Planned Files
| File | Purpose |
|------|---------|
| `TelemetryService.h/.cpp` | Event tracking |
| `CrashReporter.h/.cpp` | Crash dump capture |

## Tasks (Priority Order)
- [ ] **TEL1.** Implement `TelemetryService` with `trackEvent(name, properties)`.
- [ ] **TEL2.** Add opt-in consent dialog on first launch.
- [ ] **TEL3.** Implement crash reporter (minidump on Windows, coredump on POSIX).
- [ ] **TEL4.** Batch telemetry and send on idle / app exit.
- [ ] **TEL5.** Sanitize PII from telemetry events (no file paths, usernames, etc.).

## Related
- `src/core/updates/TODO.md` — update check can use telemetry
- `docs/` — privacy / telemetry policy
