function(add_qt_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE video_tools_pcap Qt6::Test Qt6::Core)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_qt_test(test_checksum)
add_qt_test(test_pcap_roundtrip)

add_executable(test_player_timing test_player_timing.cpp)
target_link_libraries(test_player_timing PRIVATE video_tools_core Qt6::Test Qt6::Core)
add_test(NAME test_player_timing COMMAND test_player_timing)
