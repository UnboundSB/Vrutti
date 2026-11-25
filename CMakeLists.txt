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
    src/core/utils/Inflate.cpp
    src/core/utils/ZipArchive.cpp
    src/core/utils/HttpDownload.cpp
    src/core/utils/ProcessRunner.cpp
    src/core/extensions/ExtensionService.cpp
    src/core/ipc/IPCClient.cpp
    src/core/ipc/VSCodeProtocol.cpp
    src/core/ipc/BridgeProtocol.cpp
    src/core/ipc/ExtensionHostSpawner.cpp
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
        ole32 comctl32 oleaut32 uuid mpr version shlwapi user32 advapi32 wininet
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

# The installation of an extension: the archive is read safely, the manifest
# is checked, and a hostile archive does not get in.
add_executable(vrutti_installer_test src/tests/integration/ExtensionInstallerTest.cpp)
target_link_libraries(vrutti_installer_test PRIVATE vrutti_core)
add_test(NAME ExtensionInstallerTest COMMAND vrutti_installer_test)

# The installation of an extension, seen from the protocol: the extension
# host has to learn about the extension that was installed.
add_executable(vrutti_install_protocol_test src/tests/integration/ExtensionInstallProtocolTest.cpp)
target_link_libraries(vrutti_install_protocol_test PRIVATE vrutti_core)
add_test(NAME ExtensionInstallProtocolTest COMMAND vrutti_install_protocol_test)

# The whole chain: a real extension is installed by the core and runs in the
# real extension host.
add_executable(vrutti_install_e2e_test src/tests/integration/ExtensionInstallE2ETest.cpp)
target_link_libraries(vrutti_install_e2e_test PRIVATE vrutti_core)
add_test(NAME ExtensionInstallE2ETest COMMAND vrutti_install_e2e_test)
set_tests_properties(ExtensionInstallE2ETest PROPERTIES TIMEOUT 180)

# The JSON layer under hostile input: random text, text that is cut in half
# and text that is nested very deeply.
add_executable(vrutti_json_fuzz_test src/tests/integration/JsonFuzzTest.cpp)
target_link_libraries(vrutti_json_fuzz_test PRIVATE vrutti_core)
add_test(NAME JsonFuzzTest COMMAND vrutti_json_fuzz_test)
set_tests_properties(JsonFuzzTest PROPERTIES TIMEOUT 300)

# The pipe under hostile input: frames that are not JSON, frames that never
# end and calls that are never answered.
add_executable(vrutti_ipc_fuzz_test src/tests/integration/IpcFuzzTest.cpp)
target_link_libraries(vrutti_ipc_fuzz_test PRIVATE vrutti_core)
add_test(NAME IpcFuzzTest COMMAND vrutti_ipc_fuzz_test)
set_tests_properties(IpcFuzzTest PROPERTIES TIMEOUT 300)

# The parts of the core that hold data: the encoding of the binary payloads,
# the paths of the installation, the version range of an extension, the
# files of the user, the text of a document and the settings.
add_executable(vrutti_core_hardening_test src/tests/integration/CoreHardeningTest.cpp)
target_link_libraries(vrutti_core_hardening_test PRIVATE vrutti_core)
add_test(NAME CoreHardeningTest COMMAND vrutti_core_hardening_test)
set_tests_properties(CoreHardeningTest PROPERTIES TIMEOUT 300)

# The bridge between the page and the core: the page may only send a
# message that is small enough, that names a method that the core knows,
# and whose parameters have the shape that the core expects.
add_executable(vrutti_bridge_test src/tests/integration/BridgeProtocolTest.cpp)
target_link_libraries(vrutti_bridge_test PRIVATE vrutti_core)
add_test(NAME BridgeProtocolTest COMMAND vrutti_bridge_test)
set_tests_properties(BridgeProtocolTest PROPERTIES TIMEOUT 300)

# The calls of the page and their arguments. The harness reads the source of
# the window as well: a binding that reads its arguments with the wrong
# reader makes a feature of the page stop working without a word.
add_executable(vrutti_window_bridge_test src/tests/integration/WindowBridgeTest.cpp)
target_link_libraries(vrutti_window_bridge_test PRIVATE vrutti_core)
target_compile_definitions(vrutti_window_bridge_test PRIVATE
    VRUTTI_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME WindowBridgeTest COMMAND vrutti_window_bridge_test)
set_tests_properties(WindowBridgeTest PROPERTIES TIMEOUT 300)

# The search plugin: it reads the files of the user, so it has to start its
# program without a shell and it has to keep its hands off the files that
# are too big.
add_executable(vrutti_search_test src/tests/integration/SearchPluginTest.cpp)
target_link_libraries(vrutti_search_test PRIVATE vrutti_core)
add_dependencies(vrutti_search_test vrutti_search)
add_test(NAME SearchPluginTest COMMAND vrutti_search_test)
set_tests_properties(SearchPluginTest PROPERTIES TIMEOUT 300)

# The runner of the programs and the splitter of a command line. The source
# control of the editor used to ask git through a shell, and a shell does
# not know what a backslash in front of a quote means: every question of the
# source control failed.
add_executable(vrutti_process_runner_test src/tests/integration/ProcessRunnerTest.cpp)
target_link_libraries(vrutti_process_runner_test PRIVATE vrutti_core)
add_test(NAME ProcessRunnerTest COMMAND vrutti_process_runner_test)
set_tests_properties(ProcessRunnerTest PROPERTIES TIMEOUT 300)





