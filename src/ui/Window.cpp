#include "Window.h"
#include "FileExplorer.h"
#include "EditorView.h"
#include "../core/fs/Workspace.h"
#include <iostream>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

namespace vrutti::ui {

    Window::Window(int width, int height, const std::string& title)
        : m_width(width), m_height(height), m_title(title), m_glfwWindow(nullptr) {
        m_workspace = std::make_unique<core::fs::Workspace>(std::filesystem::current_path().string());
        m_fileExplorer = std::make_unique<FileExplorer>();
        m_editorView = std::make_unique<EditorView>();
    }

    Window::~Window() {
        shutdown();
    }

    bool Window::init() {
        std::cout << "[UI] Initializing GLFW Window: " << m_title << " (" << m_width << "x" << m_height << ")" << std::endl;
        
        if (!glfwInit()) {
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_glfwWindow = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
        if (!m_glfwWindow) {
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent((GLFWwindow*)m_glfwWindow);
        glfwSwapInterval(1); // Enable vsync

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)m_glfwWindow, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        return true;
    }

    void Window::renderFrame() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        if (m_fileExplorer && m_workspace) {
            m_fileExplorer->render(*m_workspace);
        }

        if (m_editorView) {
            m_editorView->render();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize((GLFWwindow*)m_glfwWindow, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers((GLFWwindow*)m_glfwWindow);
    }

    void Window::run() {
        std::cout << "[UI] Entering main render loop..." << std::endl;
        while (!glfwWindowShouldClose((GLFWwindow*)m_glfwWindow)) {
            glfwPollEvents();
            renderFrame();
        }
    }

    void Window::shutdown() {
        if (!m_glfwWindow) return;
        std::cout << "[UI] Shutting down UI context." << std::endl;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow((GLFWwindow*)m_glfwWindow);
        glfwTerminate();
        m_glfwWindow = nullptr;
    }

} // namespace vrutti::ui
