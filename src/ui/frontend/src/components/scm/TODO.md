# Vrutti IDE — src/ui/frontend/src/components/scm/ TODO

**Scope:** `vrutti_ide/src/ui/frontend/src/components/scm/` only. Parent `vrutti/` never touched.

## Purpose
Source control management: git integration, git graph visualization, diff viewing.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `vrutti-scm.ts` | SCM component | **Security flaw** (unsafeHTML) |
| `vrutti-git-graph.ts` | Git DAG visualization | Present |
| `scmModel.ts` | SCM data model | Present |

## Critical Issues
- [ ] **SCM1.** Fix `unsafeHTML(item.iconContent)` XSS vulnerability — sanitize with DOMPurify.

## Tasks (Priority Order)
- [ ] **SCM2.** Implement git operations via C++ process spawning (git log, diff, status).
- [ ] **SCM3.** Wire `vrutti-git-graph.ts` to real git log output.
- [ ] **SCM4.** Implement diff viewer (file changes side-by-side).
- [ ] **SCM5.** Implement commit, push, pull, branch operations.
- [ ] **SCM6.** Integrate with `vrutti-diff-editor.ts` for diff display.

## Related
- `src/core/plugins/TODO.md` — potential git plugin
- `src/ui/frontend/src/components/vrutti-diff-editor.ts` — diff editor component
