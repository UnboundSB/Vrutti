#include "Window.h"
#include <iostream>

// Stub implementation for now until GLFW is fully linked in CMake
namespace vrutti::ui {

    Window::Window(int width, int height, const std::string& title)
        : m_width(width), m_height(height), m_title(title), m_glfwWindow(nullptr) {
    }

    Window::~Window() {
        shutdown();
    }

    bool Window::init() {
        std::cout << "[UI] Initializing GLFW Window: " << m_title << " (" << m_width << "x" << m_height << ")" << std::endl;
        // GLFW init, window creation, ImGui context setup would go here
        return true;
    }

    void Window::renderFrame() {
        // ImGui_ImplOpenGL3_NewFrame();
        // ImGui_ImplGlfw_NewFrame();
        // ImGui::NewFrame();
        
        // ImGui::ShowDemoWindow(); // Test UI

        // ImGui::Render();
        // glClear(GL_COLOR_BUFFER_BIT);
        // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        // glfwSwapBuffers(window);
    }

    void Window::run() {
        std::cout << "[UI] Entering main render loop..." << std::endl;
        // while (!glfwWindowShouldClose(m_glfwWindow)) {
        //     glfwPollEvents();
        //     renderFrame();
        // }
    }

    void Window::shutdown() {
        std::cout << "[UI] Shutting down UI context." << std::endl;
        // ImGui_ImplOpenGL3_Shutdown();
        // ImGui_ImplGlfw_Shutdown();
        // ImGui::DestroyContext();
        // glfwDestroyWindow();
        // glfwTerminate();
    }

} // namespace vrutti::ui
