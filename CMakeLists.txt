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
    src/core/config/SettingsManager.cpp
    src/core/plugins/PluginLoader.cpp
    src/core/terminal/TerminalProcess.cpp
    src/core/terminal/ReplProcess.cpp
    src/ui/compositor/Window.cpp
)
target_include_directories(vrutti_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# On Windows, link the libraries required by webview.h
if(WIN32)
    target_link_libraries(vrutti_core PUBLIC
        ole32 comctl32 oleaut32 uuid mpr version shlwapi user32 advapi32
    )
    target_include_directories(vrutti_core PUBLIC ${webview2_SOURCE_DIR}/build/native/include)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WEBKIT2GTK REQUIRED gtk+-3.0 webkit2gtk-4.0)
    target_include_directories(vrutti_core PUBLIC ${WEBKIT2GTK_INCLUDE_DIRS})
    target_link_libraries(vrutti_core PUBLIC ${WEBKIT2GTK_LIBRARIES})
    target_compile_options(vrutti_core PUBLIC ${WEBKIT2GTK_CFLAGS})
endif()

# Main Executable
add_executable(vrutti_app src/app/main.cpp src/app/vrutti_app.rc)
target_link_libraries(vrutti_app PRIVATE vrutti_core)

# Lazy Loaded Search DLL
add_library(vrutti_search SHARED
    src/plugins/search/SearchPlugin.cpp
)
target_link_libraries(vrutti_search PRIVATE vrutti_core)

# Rigorous Unit Testing Pipeline
enable_testing()

add_executable(vrutti_test test/main.cpp test/test_memory.cpp test/test_concurrency.cpp test/test_editor.cpp)
target_link_libraries(vrutti_test PRIVATE vrutti_core)
add_test(NAME CoreTests COMMAND vrutti_test)
# Integration Testing (Mocks)





