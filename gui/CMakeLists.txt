# ============================================================================
# gui/CMakeLists.txt
# Builds the Qt6 GUI frontend (eds-gui).
# Links against eds_core (the static library containing all command logic)
# and Qt6::Widgets.
# ============================================================================

find_package(Qt6 COMPONENTS Widgets REQUIRED)
qt_standard_project_setup()

set(GUI_SOURCES
    main.cpp
    MainWindow.cpp      MainWindow.h
    TerminalWidget.cpp  TerminalWidget.h
    CommandHighlighter.cpp CommandHighlighter.h
    CommandPalette.cpp  CommandPalette.h
)

qt_add_executable(eds-gui ${GUI_SOURCES})

target_link_libraries(eds-gui PRIVATE
    Qt6::Widgets
    eds_core
)

target_include_directories(eds-gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# macOS: set proper bundle info
if(APPLE)
    set_target_properties(eds-gui PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.eds.gui"
        MACOSX_BUNDLE_BUNDLE_NAME "EDS"
    )
endif()
