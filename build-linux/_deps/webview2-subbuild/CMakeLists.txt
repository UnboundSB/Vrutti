# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.22.1)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(webview2-populate NONE)



include(ExternalProject)
ExternalProject_Add(webview2-populate
                     "UPDATE_DISCONNECTED" "False" "URL" "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2592.51"
                    SOURCE_DIR          "/app/build-linux/_deps/webview2-src"
                    BINARY_DIR          "/app/build-linux/_deps/webview2-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)


