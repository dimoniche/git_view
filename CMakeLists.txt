cmake_minimum_required(VERSION 3.16)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" GIT_VIEW_VERSION)
string(STRIP "${GIT_VIEW_VERSION}" GIT_VIEW_VERSION)

project(git_view VERSION ${GIT_VIEW_VERSION} LANGUAGES C CXX)

set(GIT_VIEW_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GIT_VIEW_GENERATED_DIR}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/git_view_version.h.in"
    "${GIT_VIEW_GENERATED_DIR}/git_view_version.h"
    @ONLY
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(APPLE AND NOT DEFINED CMAKE_PREFIX_PATH)
    foreach(_qt_prefix IN ITEMS "/opt/homebrew/opt/qt@6" "/usr/local/opt/qt@6")
        if(EXISTS "${_qt_prefix}/lib/cmake/Qt6/Qt6Config.cmake")
            list(APPEND CMAKE_PREFIX_PATH "${_qt_prefix}")
        endif()
    endforeach()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

if(UNIX)
    add_subdirectory(third_party/libvterm)
endif()

add_subdirectory(src)

if(UNIX)
    add_subdirectory(integrations)
endif()

option(GIT_VIEW_BUILD_TESTS "Build unit tests" ON)
if(GIT_VIEW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
