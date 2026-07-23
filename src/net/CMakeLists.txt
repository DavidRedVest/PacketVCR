add_library(video_tools_net STATIC
    PlatformSocket.h
    PlatformSocket.cpp
    NetworkInterfaceInfo.h
    NetworkInterfaceInfo.cpp
    IPv4Address.h
    IPv4Address.cpp
)

target_include_directories(video_tools_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(video_tools_net PUBLIC cxx_std_17)

if(WIN32)
    target_link_libraries(video_tools_net PUBLIC ws2_32 iphlpapi)
endif()
