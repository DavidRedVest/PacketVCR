add_library(video_tools_pcap STATIC
    PcapFormat.h
    FakeHeaders.h
    FakeHeaders.cpp
    PcapWriter.h
    PcapWriter.cpp
    PcapReader.h
    PcapReader.cpp
)

target_include_directories(video_tools_pcap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(video_tools_pcap PUBLIC cxx_std_17)
