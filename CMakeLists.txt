cmake_minimum_required(VERSION 3.21)
project(video_tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt6 is not on a standard search path on this machine; point CMake at it
# with e.g.:
#   cmake -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/macos
find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets Test)
qt_standard_project_setup()

add_subdirectory(src/pcap)
add_subdirectory(src/net)
add_subdirectory(src/core)
add_subdirectory(src/cli)
add_subdirectory(src/gui)

enable_testing()
add_subdirectory(tests)
