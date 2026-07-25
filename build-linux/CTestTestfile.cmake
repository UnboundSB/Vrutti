# CMake generated Testfile for 
# Source directory: /app
# Build directory: /app/build-linux
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[MemoryTests]=] "/app/build-linux/test_memory")
set_tests_properties([=[MemoryTests]=] PROPERTIES  _BACKTRACE_TRIPLES "/app/CMakeLists.txt;72;add_test;/app/CMakeLists.txt;0;")
add_test([=[ConcurrencyTests]=] "/app/build-linux/test_concurrency")
set_tests_properties([=[ConcurrencyTests]=] PROPERTIES  _BACKTRACE_TRIPLES "/app/CMakeLists.txt;76;add_test;/app/CMakeLists.txt;0;")
add_test([=[FSTests]=] "/app/build-linux/test_fs")
set_tests_properties([=[FSTests]=] PROPERTIES  _BACKTRACE_TRIPLES "/app/CMakeLists.txt;80;add_test;/app/CMakeLists.txt;0;")
add_test([=[IPCBridgeTests]=] "/app/build-linux/test_ipc_bridge")
set_tests_properties([=[IPCBridgeTests]=] PROPERTIES  _BACKTRACE_TRIPLES "/app/CMakeLists.txt;85;add_test;/app/CMakeLists.txt;0;")
add_test([=[UICoreIntegrationTests]=] "/app/build-linux/test_ui_core")
set_tests_properties([=[UICoreIntegrationTests]=] PROPERTIES  _BACKTRACE_TRIPLES "/app/CMakeLists.txt;89;add_test;/app/CMakeLists.txt;0;")
