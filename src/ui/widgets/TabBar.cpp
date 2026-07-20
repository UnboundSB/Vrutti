#include "TabBar.h"
#include <imgui.h>

namespace vrutti::ui::widgets {

    void TabBar::render(const std::vector<std::string>& openFiles) {
        if (openFiles.empty()) return;

        if (ImGui::BeginTabBar("EditorTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll)) {
            for (size_t i = 0; i < openFiles.size(); ++i) {
                bool isOpen = true; // In a real app, track closed tabs to remove them from openFiles
                
                // Render each individual tab
                if (ImGui::BeginTabItem(openFiles[i].c_str(), &isOpen, i == m_activeTabIndex ? ImGuiTabItemFlags_SetSelected : 0)) {
                    m_activeTabIndex = static_cast<int>(i);
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }

}
