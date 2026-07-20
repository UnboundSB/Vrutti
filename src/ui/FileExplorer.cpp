#include "FileExplorer.h"
#include "../core/fs/Workspace.h"
#include <imgui.h>

namespace vrutti::ui {

    void FileExplorer::render(core::fs::Workspace& workspace) {
        ImGui::Begin("Explorer");
        renderNode(workspace, workspace.getRootMutable());
        ImGui::End();
    }

    void FileExplorer::renderNode(core::fs::Workspace& workspace, core::fs::FileNode& node) {
        if (node.isDirectory) {
            // Lazy load contents when expanded
            if (ImGui::TreeNodeEx(node.name.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
                if (!node.isScanned) {
                    workspace.scanDirectory(node);
                }
                for (auto& child : node.children) {
                    renderNode(workspace, child);
                }
                ImGui::TreePop();
            }
        } else {
            ImGui::TreeNodeEx(node.name.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
        }
    }

} // namespace vrutti::ui
