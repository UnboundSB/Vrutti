cmake_minimum_required(VERSION 3.20)
project(VruttiIDE VERSION 0.1.0 LANGUAGES CXX C)
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set compile options for high performance and strict warnings
if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O3 -flto)
    add_link_options(-static -static-libgcc -static-libstdc++)
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
    src/core/editor/DocumentStore.cpp
    src/core/fs/URI.cpp
    src/core/fs/Path.cpp
    src/core/fs/Glob.cpp
    src/core/fs/Workspace.cpp
    src/core/fs/FileService.cpp
    src/core/utils/Json.cpp
    src/core/utils/Paths.cpp
    src/core/utils/ErrorCodes.cpp
    src/core/utils/StringPool.cpp
    src/core/utils/LineScanner.cpp
    src/core/ipc/IPCClient.cpp
    src/core/ipc/VSCodeProtocol.cpp
    src/core/config/SettingsManager.cpp
    src/core/plugins/PluginLoader.cpp
    src/core/plugins/ExtensionScanner.cpp
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
set_target_properties(vrutti_app PROPERTIES OUTPUT_NAME "vrutti")
target_link_libraries(vrutti_app PRIVATE vrutti_core)

# Lazy Loaded Search DLL
add_library(vrutti_search SHARED
    src/plugins/search/SearchPlugin.cpp
)
set_target_properties(vrutti_search PROPERTIES PREFIX "")
target_link_libraries(vrutti_search PRIVATE vrutti_core)

# Plugin DLLs
function(add_vrutti_plugin name source_file)
    add_library(${name} SHARED src/core/plugins/${source_file})
    set_target_properties(${name} PROPERTIES PREFIX "")
    target_link_libraries(${name} PRIVATE vrutti_core)
endfunction()

add_vrutti_plugin(VSCodeExtHostService VSCodeExtHostService.cpp)
add_vrutti_plugin(ExtensionManager ExtensionManager.cpp)
add_vrutti_plugin(DocumentSyncService DocumentSyncService.cpp)
add_vrutti_plugin(ExtensionLifecycleManager ExtensionLifecycleManager.cpp)
add_vrutti_plugin(DebugAdapterService DebugAdapterService.cpp)
add_vrutti_plugin(TerminalService TerminalService.cpp)
add_vrutti_plugin(CommandHandlerService CommandHandlerService.cpp)
add_vrutti_plugin(DiagnosticService DiagnosticService.cpp)
add_vrutti_plugin(FileSystemService FileSystemService.cpp)
add_vrutti_plugin(LanguageFeatureService LanguageFeatureService.cpp)
add_vrutti_plugin(MediaController MediaController.cpp)
add_vrutti_plugin(VSXRegistryService VSXRegistryService.cpp)
target_link_libraries(VSXRegistryService PRIVATE wininet)
add_vrutti_plugin(WorkspaceService WorkspaceService.cpp)
add_vrutti_plugin(ThemeService ThemeService.cpp)

# Rigorous Unit Testing Pipeline
enable_testing()

add_executable(vrutti_integration_test src/tests/integration/ExtHostIntegrationTest.cpp)
target_link_libraries(vrutti_integration_test PRIVATE vrutti_core)
add_test(NAME IntegrationTest COMMAND vrutti_integration_test)

add_executable(vrutti_theme_test src/tests/integration/ThemeServiceTest.cpp)
target_link_libraries(vrutti_theme_test PRIVATE vrutti_core)
add_dependencies(vrutti_theme_test ThemeService)
add_test(NAME ThemeServiceTest COMMAND vrutti_theme_test)

# The contract of the extension host router: a call without an implementation
# must be answered with the benign default of its return type, never with an
# error. The extension host turns an error into an unhandled rejection and
# marks the extension as broken.
add_executable(vrutti_protocol_fallback_test src/tests/integration/ProtocolFallbackTest.cpp)
target_link_libraries(vrutti_protocol_fallback_test PRIVATE vrutti_core)
add_test(NAME ProtocolFallbackTest COMMAND vrutti_protocol_fallback_test)

# `vscode.workspace.fs`: the way an extension reads and writes a file.
add_executable(vrutti_exthost_fs_test src/tests/integration/ExthostFileSystemTest.cpp)
target_link_libraries(vrutti_exthost_fs_test PRIVATE vrutti_core)
add_test(NAME ExthostFileSystemTest COMMAND vrutti_exthost_fs_test)

# `vscode.workspace.openTextDocument()`: the way an extension opens, creates
# and saves a text document.
add_executable(vrutti_documents_test src/tests/integration/MainThreadDocumentsTest.cpp)
target_link_libraries(vrutti_documents_test PRIVATE vrutti_core)
add_test(NAME MainThreadDocumentsTest COMMAND vrutti_documents_test)





