#pragma once
#include <string>

#include <memory>

namespace vrutti::core::fs {
    class Workspace;
}

namespace vrutti::ui {
    class FileExplorer;
    class EditorView;

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

        std::unique_ptr<vrutti::core::fs::Workspace> m_workspace;
        std::unique_ptr<FileExplorer> m_fileExplorer;
        std::unique_ptr<EditorView> m_editorView;

        void renderFrame();
    };

} // namespace vrutti::ui
