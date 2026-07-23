add_executable(recorder_cli recorder_cli_main.cpp)
target_link_libraries(recorder_cli PRIVATE video_tools_core)

add_executable(player_cli player_cli_main.cpp)
target_link_libraries(player_cli PRIVATE video_tools_core)

install(TARGETS recorder_cli player_cli RUNTIME DESTINATION .)
