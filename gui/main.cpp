// ============================================================================
// File:    gui/main.cpp
// Summary: Entry point for the Qt GUI frontend of the Embedded Diagnostic
//          Shell. Creates a QApplication, applies the dark theme by default,
//          and launches the MainWindow. The shell core (eds_core) is used
//          as a library — all command logic is shared with the CLI frontend.
// ============================================================================

#include "MainWindow.h"
#include <QApplication>
#include <QFile>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ShellX - Multithreaded Linux Shell");
    app.setOrganizationName("ShellX");

    QFont monospace("Menlo", 13);
    monospace.setStyleHint(QFont::Monospace);
    app.setFont(monospace);

    MainWindow window;
    window.resize(1100, 700);
    window.show();

    return app.exec();
}
