cmake_minimum_required(VERSION 3.20)
project(VruttiIDE VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set compile options for high performance and strict warnings
if(MSVC)
    add_compile_options(/W4 /WX /O2)
else()
    add_compile_options(-Wall -Wextra -Werror -O3 -flto)
endif()

# Core Module
add_library(vrutti_core STATIC
    src/core/memory/ArenaAllocator.cpp
    src/core/concurrency/ThreadPool.cpp
    src/core/editor/PieceTable.cpp
)
target_include_directories(vrutti_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Main Executable
add_executable(vrutti_app src/app/main.cpp)
target_link_libraries(vrutti_app PRIVATE vrutti_core)

# TODO: Add UI/Compositor and Bridge dependencies later
