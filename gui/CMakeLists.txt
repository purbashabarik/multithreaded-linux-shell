# ============================================================================
# gui/CMakeLists.txt
# Builds the Qt6 GUI frontend (shellx-gui).
# Links against shellx_core (the static library containing all command logic)
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

qt_add_executable(shellx-gui ${GUI_SOURCES})

target_link_libraries(shellx-gui PRIVATE
    Qt6::Widgets
    shellx_core
)

target_include_directories(shellx-gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# macOS: set proper bundle info
if(APPLE)
    set_target_properties(shellx-gui PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.shellx.gui"
        MACOSX_BUNDLE_BUNDLE_NAME "ShellX"
    )
endif()
