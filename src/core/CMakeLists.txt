find_package(Threads REQUIRED)

add_library(video_tools_core STATIC
    Recorder.h
    Recorder.cpp
    Player.h
    Player.cpp
)

target_include_directories(video_tools_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(video_tools_core PUBLIC cxx_std_17)
target_link_libraries(video_tools_core PUBLIC video_tools_pcap video_tools_net Threads::Threads)
