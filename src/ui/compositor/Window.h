#pragma once
#include <string>

namespace vrutti::ui {
    class Window {
    public:
        Window(int width, int height, const std::string& title);
        ~Window();

        bool init();
        void run();
        void shutdown();
        bool shouldClose() const;

    private:
        int m_width;
        int m_height;
        std::string m_title;
        void* m_windowHandle; // Type erased webview handle
    };
}
