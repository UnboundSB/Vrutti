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

    private:
        int m_width;
        int m_height;
        std::string m_title;
        void* m_glfwWindow; // Type erased to avoid leaking GLFW headers everywhere

        void renderFrame();
    };

} // namespace vrutti::ui
