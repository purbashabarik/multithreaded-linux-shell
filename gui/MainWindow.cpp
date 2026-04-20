// ============================================================================
// File:    MainWindow.cpp
// Summary: Implementation of the main window. Sets up the file tree dock,
//          tab widget, menu bar, keyboard shortcuts, and theme toggling.
//          Each new tab creates a fresh TerminalWidget connected to the
//          shell core. The file tree updates when the user cds.
// ============================================================================

#include "MainWindow.h"
#include "TerminalWidget.h"
#include "CommandPalette.h"
#include <QMenuBar>
#include <QPushButton>
#include <QShortcut>
#include <QKeySequence>
#include <QDir>
#include <QApplication>
#include <QStyleFactory>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_darkMode(true), m_tabCounter(0)
{
    setWindowTitle("EDS - Embedded Diagnostic Shell");

    setupFileTree();
    setupTabs();
    setupMenuBar();

    applyDarkTheme();

    new QShortcut(QKeySequence("Ctrl+Shift+P"), this, SLOT(openCommandPalette()));
    new QShortcut(QKeySequence("Ctrl+T"), this, SLOT(addNewTab()));

    addNewTab();
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("New Tab", QKeySequence("Ctrl+T"), this, &MainWindow::addNewTab);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", QKeySequence("Ctrl+Q"), this, &QMainWindow::close);

    QMenu *viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("Toggle File Tree", this, &MainWindow::toggleFileTree);
    viewMenu->addAction("Toggle Theme", QKeySequence("Ctrl+Shift+T"), this, &MainWindow::toggleTheme);
    viewMenu->addSeparator();
    viewMenu->addAction("Command Palette", QKeySequence("Ctrl+Shift+P"), this, &MainWindow::openCommandPalette);

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("About", [this]() {
        QMessageBox::about(this, "About EDS",
            "Embedded Diagnostic Shell (EDS)\n\n"
            "A multi-threaded C++ shell with a Qt GUI frontend.\n"
            "Supports: built-in commands, external programs,\n"
            "pipes, I/O redirection, syntax highlighting,\n"
            "file tree, tabbed sessions, and command palette.\n\n"
            "Built as an OOPD portfolio project.");
    });
}

void MainWindow::setupFileTree()
{
    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setRootPath(QDir::currentPath());
    m_fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

    m_fileTree = new QTreeView;
    m_fileTree->setModel(m_fileModel);
    m_fileTree->setRootIndex(m_fileModel->index(QDir::currentPath()));
    m_fileTree->setHeaderHidden(true);
    // Show only the name column, hide size/type/date
    for (int i = 1; i < m_fileModel->columnCount(); ++i)
        m_fileTree->hideColumn(i);
    m_fileTree->setMinimumWidth(200);

    m_fileTreeDock = new QDockWidget("File Tree", this);
    m_fileTreeDock->setWidget(m_fileTree);
    m_fileTreeDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_fileTreeDock);
}

void MainWindow::setupTabs()
{
    m_tabs = new QTabWidget;
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);

    // "+" button to add new tabs
    auto *addButton = new QPushButton("+");
    addButton->setFixedSize(28, 28);
    addButton->setFlat(true);
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addNewTab);
    m_tabs->setCornerWidget(addButton, Qt::TopRightCorner);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);

    setCentralWidget(m_tabs);
}

void MainWindow::addNewTab()
{
    m_tabCounter++;
    auto *terminal = new TerminalWidget(this);
    connect(terminal, &TerminalWidget::directoryChanged, this, &MainWindow::onDirectoryChanged);

    int index = m_tabs->addTab(terminal, QString("Shell %1").arg(m_tabCounter));
    m_tabs->setCurrentIndex(index);
    terminal->focusInput();
}

void MainWindow::closeTab(int index)
{
    if (m_tabs->count() <= 1)
    {
        addNewTab();
    }
    QWidget *w = m_tabs->widget(index);
    m_tabs->removeTab(index);
    w->deleteLater();
}

void MainWindow::onDirectoryChanged(const QString &newDir)
{
    m_fileModel->setRootPath(newDir);
    m_fileTree->setRootIndex(m_fileModel->index(newDir));
}

void MainWindow::toggleFileTree()
{
    m_fileTreeDock->setVisible(!m_fileTreeDock->isVisible());
}

void MainWindow::toggleTheme()
{
    m_darkMode = !m_darkMode;
    if (m_darkMode)
        applyDarkTheme();
    else
        applyLightTheme();
}

void MainWindow::applyDarkTheme()
{
    qApp->setStyle(QStyleFactory::create("Fusion"));
    QPalette p;
    p.setColor(QPalette::Window, QColor(30, 30, 30));
    p.setColor(QPalette::WindowText, QColor(212, 212, 212));
    p.setColor(QPalette::Base, QColor(20, 20, 20));
    p.setColor(QPalette::AlternateBase, QColor(40, 40, 40));
    p.setColor(QPalette::ToolTipBase, QColor(50, 50, 50));
    p.setColor(QPalette::ToolTipText, QColor(212, 212, 212));
    p.setColor(QPalette::Text, QColor(212, 212, 212));
    p.setColor(QPalette::Button, QColor(45, 45, 45));
    p.setColor(QPalette::ButtonText, QColor(212, 212, 212));
    p.setColor(QPalette::BrightText, QColor(255, 100, 100));
    p.setColor(QPalette::Link, QColor(86, 156, 214));
    p.setColor(QPalette::Highlight, QColor(38, 79, 120));
    p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    qApp->setPalette(p);
}

void MainWindow::applyLightTheme()
{
    qApp->setStyle(QStyleFactory::create("Fusion"));
    QPalette p;
    p.setColor(QPalette::Window, QColor(240, 240, 240));
    p.setColor(QPalette::WindowText, QColor(30, 30, 30));
    p.setColor(QPalette::Base, QColor(255, 255, 255));
    p.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
    p.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
    p.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
    p.setColor(QPalette::Text, QColor(30, 30, 30));
    p.setColor(QPalette::Button, QColor(225, 225, 225));
    p.setColor(QPalette::ButtonText, QColor(30, 30, 30));
    p.setColor(QPalette::BrightText, QColor(200, 0, 0));
    p.setColor(QPalette::Link, QColor(0, 100, 200));
    p.setColor(QPalette::Highlight, QColor(0, 120, 215));
    p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    qApp->setPalette(p);
}

void MainWindow::openCommandPalette()
{
    auto *active = qobject_cast<TerminalWidget *>(m_tabs->currentWidget());
    if (!active) return;

    CommandPalette palette(active->commandHistory(), this);
    if (palette.exec() == QDialog::Accepted)
    {
        QString selected = palette.selectedCommand();
        if (!selected.isEmpty())
        {
            active->runCommand(selected);
        }
    }
}
