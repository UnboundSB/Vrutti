
if(NOT "/app/build-linux/_deps/imgui-subbuild/imgui-populate-prefix/src/imgui-populate-stamp/imgui-populate-gitinfo.txt" IS_NEWER_THAN "/app/build-linux/_deps/imgui-subbuild/imgui-populate-prefix/src/imgui-populate-stamp/imgui-populate-gitclone-lastrun.txt")
  message(STATUS "Avoiding repeated git clone, stamp file is up to date: '/app/build-linux/_deps/imgui-subbuild/imgui-populate-prefix/src/imgui-populate-stamp/imgui-populate-gitclone-lastrun.txt'")
  return()
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E rm -rf "/app/build-linux/_deps/imgui-src"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to remove directory: '/app/build-linux/_deps/imgui-src'")
endif()

# try the clone 3 times in case there is an odd git clone issue
set(error_code 1)
set(number_of_tries 0)
while(error_code AND number_of_tries LESS 3)
  execute_process(
    COMMAND "/usr/bin/git"  clone --no-checkout --config "advice.detachedHead=false" "https://github.com/ocornut/imgui.git" "imgui-src"
    WORKING_DIRECTORY "/app/build-linux/_deps"
    RESULT_VARIABLE error_code
    )
  math(EXPR number_of_tries "${number_of_tries} + 1")
endwhile()
if(number_of_tries GREATER 1)
  message(STATUS "Had to git clone more than once:
          ${number_of_tries} times.")
endif()
if(error_code)
  message(FATAL_ERROR "Failed to clone repository: 'https://github.com/ocornut/imgui.git'")
endif()

execute_process(
  COMMAND "/usr/bin/git"  checkout v1.90.8-docking --
  WORKING_DIRECTORY "/app/build-linux/_deps/imgui-src"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to checkout tag: 'v1.90.8-docking'")
endif()

set(init_submodules TRUE)
if(init_submodules)
  execute_process(
    COMMAND "/usr/bin/git"  submodule update --recursive --init 
    WORKING_DIRECTORY "/app/build-linux/_deps/imgui-src"
    RESULT_VARIABLE error_code
    )
endif()
if(error_code)
  message(FATAL_ERROR "Failed to update submodules in: '/app/build-linux/_deps/imgui-src'")
endif()

# Complete success, update the script-last-run stamp file:
#
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy
    "/app/build-linux/_deps/imgui-subbuild/imgui-populate-prefix/src/imgui-populate-stamp/imgui-populate-gitinfo.txt"
    "/app/build-linux/_deps/imgui-subbuild/imgui-populate-prefix/src/imgui-populate-stamp/imgui-populate-gitclone-lastrun.txt"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to copy script-last-run stamp file: '/app/build-linux/_deps/imgui-subbuild/imgui-populate-prefix/src/imgui-populate-stamp/imgui-populate-gitclone-lastrun.txt'")
endif()

