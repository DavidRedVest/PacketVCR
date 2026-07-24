set(video_tools_gui_sources
    main.cpp
    MainWindow.h
    MainWindow.cpp
    RecorderPanel.h
    RecorderPanel.cpp
    PlayerPanel.h
    PlayerPanel.cpp
    InterfaceModel.h
    InterfaceModel.cpp
    resources.qrc
)

# icons/PacketVCR.icns is the macOS app bundle icon (Finder/Dock); it isn't
# compiled, just copied into Contents/Resources by the MACOSX_BUNDLE_ICON_FILE
# mechanism below. icons/app_icon.rc is the equivalent for Windows: a resource
# script embedding icons/PacketVCR.ico into the .exe (Explorer/taskbar). Both
# are pre-rendered from icons/icon_vcr.svg since neither format can reference
# an SVG directly; the runtime window/taskbar icon (all platforms) instead
# loads that SVG straight from resources.qrc so it stays crisp at any size.
# Only added on their respective platforms -- an .rc source errors out under
# non-Windows generators, and the .icns has nothing to do outside a bundle.
if(APPLE)
    set_source_files_properties(icons/PacketVCR.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    list(APPEND video_tools_gui_sources icons/PacketVCR.icns)
endif()
if(WIN32)
    list(APPEND video_tools_gui_sources icons/app_icon.rc)
endif()

qt_add_executable(video_tools_gui ${video_tools_gui_sources})

target_link_libraries(video_tools_gui PRIVATE video_tools_core Qt6::Widgets)

set_target_properties(video_tools_gui PROPERTIES
    OUTPUT_NAME "PacketVCR"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "PacketVCR"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.packetvcr.gui"
    MACOSX_BUNDLE_ICON_FILE "PacketVCR.icns"
)

install(TARGETS video_tools_gui
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)
