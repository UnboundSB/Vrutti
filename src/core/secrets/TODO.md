# Vrutti IDE — src/core/secrets/ TODO

**Scope:** `vrutti_ide/src/core/secrets/` only. Parent `vrutti/` never touched.

## Purpose
Secret / credential storage: OS-native storage for extension authentication tokens (GitHub, Azure, etc.). VS Code has a `secrets` platform service.

## Planned Files
| File | Purpose |
|------|---------|
| `SecretStorage.h/.cpp` | OS-native secret store |
| `SecretProvider.h` | Extension secret provider interface |

## Tasks (Priority Order)
- [ ] **SEC1.** Implement Windows Credential Manager backend (`CredWrite` / `CredRead`).
- [ ] **SEC2.** Implement POSIX libsecret backend (Linux/macOS).
- [ ] **SEC3.** Extension API: `secrets.store`, `secrets.get`, `secrets.delete`.
- [ ] **SEC4.** Encryption at rest for stored secrets.
- [ ] **SEC5.** Expose via IPC: `secrets/store`, `secrets/get`, `secrets/delete`.

## Related
- `src/ext/api/TODO.md` — extension API surface
- `src/core/telemetry/TODO.md` — no PII in secrets
