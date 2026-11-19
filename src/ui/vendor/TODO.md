# Vrutti IDE — src/ui/vendor/ TODO

**Scope:** `vrutti_ide/src/ui/vendor/` only. Parent `vrutti/` never touched.

## Purpose
Third-party vendor libraries: ImGui (immediate-mode GUI for native overlays), webview.h (WebView2 wrapper). Third-party code — do not modify application logic here.

## Directory Inventory
| Dir | Purpose | Status |
|-----|---------|--------|
| `imgui/` | Dear ImGui (C++ immediate GUI) | Present |
| `imgui/backends/` | ImGui OpenGL3 + GLFW backends | Present |
| `webview.h` | WebView2 single-header wrapper | Present |

## Tasks (Priority Order)
- [ ] **VND1.** Do not edit vendor files; pin exact versions.
- [ ] **VND2.** Document licenses (ImGui: MIT, webview.h: zlib/libpng).
- [ ] **VND3.** Verify `webview.h` version matches the WebView2 runtime requirements.
- [ ] **VND4.** Consider if ImGui is still needed — the main shell is Lit/WebView2; ImGui may be for native overlays only.

## Related
- `src/ui/compositor/Window.cpp` — uses webview.h and ImGui
