#pragma once

#include <string>
#include <vector>

namespace vrutti::ui::widgets {

    class TabBar {
    public:
        // Renders the row of open file tabs
        void render(const std::vector<std::string>& openFiles);

    private:
        int m_activeTabIndex = 0;
    };

}
