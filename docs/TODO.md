# Vrutti IDE — docs/ TODO

**Scope:** `vrutti_ide/docs/` only. Parent `vrutti/` never touched.

## Purpose
Architectural documentation: single source of truth. Consolidates scattered docs from root.

## Planned Files
| File | Purpose |
|------|---------|
| `architecture.md` | Full VS Code architecture comparison (Mermaid) — moved from root |
| `system-architecture.md` | System architecture manifest — moved from root |
| `problem.md` | Known issues and security vulnerabilities — moved from root |
| `tasks.md` | Release roadmap — moved from root |
| `api-contract.md` | IPC JSON-RPC method contract |
| `component-model.md` | UI component model (Lit + C++ plugins) |
| `release-process.md` | How to cut a release |

## Tasks (Priority Order)
- [ ] **DOC1.** Move `architecture.md`, `SYSTEM_ARCHITECTURE.md`, `problem.md`, `task.md`, `tasks.md`, `summary.md`, `structure.md` from root into `docs/` (or link to them).
- [ ] **DOC2.** Write `api-contract.md` — definitive IPC method catalog.
- [ ] **DOC3.** Write `component-model.md` — how Lit components and C++ plugins interact.
- [ ] **DOC4.** Write `release-process.md` — release checklist and versioning.
- [ ] **DOC5.** Keep `docs/` in sync with code changes (update docs when architecture changes).

## Related
- `architecture.md` (root) — current architecture doc
- `scripts/TODO.md` — `gen_docs.ps1` can generate docs from source
