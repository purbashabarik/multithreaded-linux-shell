// ============================================================================
// File:    MainWindow.h
// Summary: Main application window for the EDS GUI. Layout:
//          - Left dock: file tree (QTreeView + QFileSystemModel) that
//            auto-navigates when the user runs "cd" in a terminal tab.
//          - Center: QTabWidget holding one TerminalWidget per session.
//            "+" button creates new tabs; tabs are closable.
//          - Menu bar: File, View (toggle file tree, toggle theme),
//            Settings, Help.
//          - Ctrl+Shift+P opens the CommandPalette.
//          - Ctrl+F triggers search-in-scrollback in the active tab.
//          Theming is toggled between dark and light via QApplication palette
//          and QSS stylesheet swap.
// ============================================================================

#ifndef EDS_MAIN_WINDOW_H
#define EDS_MAIN_WINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QDockWidget>
#include <QAction>

class TerminalWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

public slots:
    void addNewTab();
    void closeTab(int index);
    void onDirectoryChanged(const QString &newDir);
    void toggleFileTree();
    void toggleTheme();
    void openCommandPalette();

private:
    void setupMenuBar();
    void setupFileTree();
    void setupTabs();
    void applyDarkTheme();
    void applyLightTheme();

    QTabWidget *m_tabs;
    QDockWidget *m_fileTreeDock;
    QTreeView *m_fileTree;
    QFileSystemModel *m_fileModel;
    bool m_darkMode;
    int m_tabCounter;
};

#endif // EDS_MAIN_WINDOW_H
