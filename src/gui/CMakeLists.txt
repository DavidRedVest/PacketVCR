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
