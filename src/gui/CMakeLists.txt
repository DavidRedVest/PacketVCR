qt_add_executable(video_tools_gui
    main.cpp
    MainWindow.h
    MainWindow.cpp
    RecorderPanel.h
    RecorderPanel.cpp
    PlayerPanel.h
    PlayerPanel.cpp
    InterfaceModel.h
    InterfaceModel.cpp
)

target_link_libraries(video_tools_gui PRIVATE video_tools_core Qt6::Widgets)

set_target_properties(video_tools_gui PROPERTIES
    OUTPUT_NAME "PacketVCR"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "PacketVCR"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.packetvcr.gui"
)

install(TARGETS video_tools_gui
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)
