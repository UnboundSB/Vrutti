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

# Fetch Microsoft Edge WebView2 SDK
FetchContent_Declare(
    webview2
    URL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2592.51"
)
FetchContent_MakeAvailable(webview2)

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
)
target_include_directories(vrutti_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# On Windows, link the libraries required by webview.h
if(WIN32)
    target_link_libraries(vrutti_core PUBLIC
        ole32 comctl32 oleaut32 uuid mpr version shlwapi user32 advapi32
    )
    target_include_directories(vrutti_core PUBLIC ${webview2_SOURCE_DIR}/build/native/include)
endif()

# Main Executable
add_executable(vrutti_app src/app/main.cpp)
target_link_libraries(vrutti_app PRIVATE vrutti_core)

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




