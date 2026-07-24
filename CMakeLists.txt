cmake_minimum_required(VERSION 3.21)
project(video_tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PACKETVCR_BUILD_GUI "Build the Qt Widgets GUI" ON)
option(PACKETVCR_BUILD_TESTS "Build the Qt-based test binaries" ON)

set(packetvcr_qt_components)
if(PACKETVCR_BUILD_GUI)
    list(APPEND packetvcr_qt_components Core Widgets)
endif()
if(PACKETVCR_BUILD_TESTS)
    list(APPEND packetvcr_qt_components Core Test)
endif()
list(REMOVE_DUPLICATES packetvcr_qt_components)

if(packetvcr_qt_components)
    # Qt6 is not on a standard search path on some machines; point CMake at it
    # with e.g.:
    #   cmake -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/macos
    find_package(Qt6 REQUIRED COMPONENTS ${packetvcr_qt_components})
    qt_standard_project_setup()
endif()

add_subdirectory(src/pcap)
add_subdirectory(src/net)
add_subdirectory(src/core)
add_subdirectory(src/cli)

if(PACKETVCR_BUILD_GUI)
    add_subdirectory(src/gui)
endif()

if(PACKETVCR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
