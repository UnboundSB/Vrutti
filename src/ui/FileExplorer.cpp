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
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f)); // Light blue for folders
            bool isExpanded = ImGui::TreeNodeEx(node.name.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick);
            ImGui::PopStyleColor();
            
            if (isExpanded) {
                if (!node.isScanned) {
                    workspace.scanDirectory(node);
                }
                for (auto& child : node.children) {
                    renderNode(workspace, child);
                }
                ImGui::TreePop();
            } else {
                // Deep lazy loading: instantly free RAM when a folder is collapsed.
                // We avoid unloading the root workspace directory itself.
                if (node.isScanned && &node != &workspace.getRoot()) {
                    workspace.unloadDirectory(node);
                }
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f)); // Light gray for files
            ImGui::TreeNodeEx(node.name.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
            ImGui::PopStyleColor();
        }
    }

} // namespace vrutti::ui
