# Vrutti IDE — src/core/notifications/ TODO

**Scope:** `vrutti_ide/src/core/notifications/` only. Parent `vrutti/` never touched.

## Purpose
Notification service: toasts, alerts, and extension notifications. VS Code has a `notification` platform service.

## Planned Files
| File | Purpose |
|------|---------|
| `NotificationService.h/.cpp` | Show / dismiss notifications |
| `Notification.h` | Notification model (severity, message, actions) |

## Tasks (Priority Order)
- [ ] **NOT1.** Implement notification queue with severity levels (info, warn, error).
- [ ] **NOT2.** Frontend toast component (`vrutti-notification`) to display notifications.
- [ ] **NOT3.** Expose via IPC: `notification/show`, `notification/hide`.
- [ ] **NOT4.** Extension API for notifications (`vscode.window.showInformationMessage`, etc.).
- [ ] **NOT5.** Persistent notifications (don't dismiss on click) for critical errors.

## Related
- `src/ui/frontend/src/components/vrutti-panel.ts` — notification panel
- `src/core/ipc/TODO.md` — IPC notification methods
