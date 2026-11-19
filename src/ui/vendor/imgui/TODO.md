# Vrutti IDE — src/ui/vendor/imgui/ TODO

**Scope:** `vrutti_ide/src/ui/vendor/imgui/` only. Parent `vrutti/` never touched.

## Purpose
Dear ImGui — immediate-mode GUI library for C++. Used for native overlays/debug UI, not the main IDE shell (which is Lit/WebView2).

## Status
Third-party library — do not modify.

## Tasks (Priority Order)
- [ ] **IMG1.** Pin ImGui version (check `imconfig.h`).
- [ ] **IMG2.** Document that ImGui is for debug/native overlays only.
- [ ] **IMG3.** Verify backends compile with the project's OpenGL/GLFW setup.

## Related
- `src/ui/vendor/TODO.md` — vendor overview
- `src/ui/compositor/Window.cpp` — ImGui integration point
