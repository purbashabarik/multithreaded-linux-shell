// ============================================================================
// File:    TerminalWidget.h
// Summary: A single terminal session widget. Contains:
//          - AnsiTextEdit (read-only output pane with ANSI color rendering)
//          - QLineEdit (input line with CommandHighlighter)
//          - Search bar (Ctrl+F) for scrollback search
//          Commands are executed on a background QThread so the UI never
//          freezes. Output is streamed back via signal/slot with
//          Qt::QueuedConnection. Each tab in the MainWindow holds one
//          TerminalWidget, giving independent session state.
// ============================================================================

#ifndef EDS_TERMINAL_WIDGET_H
#define EDS_TERMINAL_WIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QStringList>
#include <QProcess>

class CommandHighlighter;

class AnsiTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit AnsiTextEdit(QWidget *parent = nullptr);
    void appendAnsi(const QString &text);

private:
    QColor ansiToColor(int code, bool bright) const;
    void applyAnsiSequence(const QString &seq, QTextCharFormat &fmt);
};

class TerminalWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget *parent = nullptr);

    void focusInput();
    void runCommand(const QString &cmd);
    QStringList commandHistory() const { return m_history; }

signals:
    void directoryChanged(const QString &newDir);

private slots:
    void onReturnPressed();
    void onSearchToggled();
    void onSearchNext();
    void onSearchPrev();
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void appendOutput(const QString &text);
    void appendPrompt();
    void executeBuiltin(const QString &cmd, const QStringList &args);
    void executeExternal(const QString &cmdLine);

    AnsiTextEdit *m_output;
    QLineEdit *m_input;
    CommandHighlighter *m_highlighter;

    // Search bar
    QWidget *m_searchBar;
    QLineEdit *m_searchInput;
    QLabel *m_searchStatus;

    QStringList m_history;
    int m_historyIndex;
    QString m_currentDir;

    QProcess *m_process;
    bool m_processRunning;
};

#endif // EDS_TERMINAL_WIDGET_H
