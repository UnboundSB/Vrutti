cmake_minimum_required(VERSION 3.20)
project(VruttiIDE VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set compile options for high performance and strict warnings
if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O3 -flto)
endif()

# TODO: Add UI/Compositor and Bridge dependencies later

include(FetchContent)

# Fetch GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Fetch ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.8-docking
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# Create a library for ImGui
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

# Core Module
add_library(vrutti_core STATIC
    src/core/memory/ArenaAllocator.cpp
    src/core/concurrency/ThreadPool.cpp
    src/core/editor/PieceTable.cpp
    src/core/fs/URI.cpp
    src/core/fs/Path.cpp
    src/core/fs/Glob.cpp
    src/core/fs/Workspace.cpp
    src/core/utils/Json.cpp
    src/core/utils/StringPool.cpp
    src/core/utils/LineScanner.cpp
    src/core/ipc/IPCClient.cpp
    src/core/plugins/PluginLoader.cpp
    src/ui/compositor/Window.cpp
    src/ui/views/Layout.cpp
    src/ui/views/EditorView.cpp
    src/ui/views/FileExplorer.cpp
    src/ui/widgets/TabBar.cpp
)
target_include_directories(vrutti_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Find OpenGL
find_package(OpenGL REQUIRED)

target_link_libraries(vrutti_core PUBLIC imgui OpenGL::GL)

# Main Executable
add_executable(vrutti_app src/app/main.cpp)
target_link_libraries(vrutti_app PRIVATE vrutti_core imgui glfw OpenGL::GL)

# Lazy Loaded Search DLL
add_library(vrutti_search SHARED
    src/plugins/search/SearchPlugin.cpp
)
target_link_libraries(vrutti_search PRIVATE vrutti_core)

# Rigorous Unit Testing Pipeline
enable_testing()

add_executable(test_memory tests/test_memory.cpp)
target_link_libraries(test_memory PRIVATE vrutti_core)
add_test(NAME MemoryTests COMMAND test_memory)

add_executable(test_concurrency tests/test_concurrency.cpp)
target_link_libraries(test_concurrency PRIVATE vrutti_core)
add_test(NAME ConcurrencyTests COMMAND test_concurrency)

add_executable(test_fs tests/test_fs.cpp)
target_link_libraries(test_fs PRIVATE vrutti_core)
add_test(NAME FSTests COMMAND test_fs)

# Integration Testing (Mocks)
add_executable(test_ipc_bridge tests/test_ipc_bridge.cpp)
target_link_libraries(test_ipc_bridge PRIVATE vrutti_core)
add_test(NAME IPCBridgeTests COMMAND test_ipc_bridge)

add_executable(test_ui_core tests/test_ui_core.cpp)
target_link_libraries(test_ui_core PRIVATE vrutti_core)
add_test(NAME UICoreIntegrationTests COMMAND test_ui_core)




