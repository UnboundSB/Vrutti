#include "EditorView.h"
#include <imgui.h>

namespace vrutti::ui {
    void EditorView::render() {
        ImGui::Begin("Vrutti Editor");
        ImGui::Text("Hello, Vrutti!");
        ImGui::End();
    }
}
