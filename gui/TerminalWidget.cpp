// ============================================================================
// File:    TerminalWidget.cpp
// Summary: Terminal session implementation. Key design decisions:
//          1. External commands run via QProcess (non-blocking, signals for
//             stdout/stderr/finished). This avoids manual fork/exec in the
//             GUI and leverages Qt's event loop for safe cross-thread output.
//          2. Built-in commands (cd, ls, exit) are handled in-process for
//             instant response.
//          3. ANSI escape sequences in command output are parsed and rendered
//             as colored text using QTextCharFormat.
//          4. Ctrl+F opens an inline search bar for scrollback.
//          5. Up/Down arrows navigate command history.
// ============================================================================

#include "TerminalWidget.h"
#include "CommandHighlighter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QShortcut>
#include <QKeyEvent>
#include <QDir>
#include <QTextBlock>
#include <QRegularExpression>
#include <QScrollBar>

// ---- AnsiTextEdit implementation -------------------------------------------

AnsiTextEdit::AnsiTextEdit(QWidget *parent) : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setMaximumBlockCount(10000);
}

QColor AnsiTextEdit::ansiToColor(int code, bool bright) const
{
    static const QColor normal[] = {
        QColor(0, 0, 0),       // 0 black
        QColor(205, 49, 49),   // 1 red
        QColor(13, 188, 121),  // 2 green
        QColor(229, 229, 16),  // 3 yellow
        QColor(36, 114, 200),  // 4 blue
        QColor(188, 63, 188),  // 5 magenta
        QColor(17, 168, 205),  // 6 cyan
        QColor(204, 204, 204), // 7 white
    };
    static const QColor brightColors[] = {
        QColor(102, 102, 102), // 0
        QColor(241, 76, 76),   // 1
        QColor(35, 209, 139),  // 2
        QColor(245, 245, 67),  // 3
        QColor(59, 142, 234),  // 4
        QColor(214, 112, 214), // 5
        QColor(41, 184, 219),  // 6
        QColor(242, 242, 242), // 7
    };
    if (code < 0 || code > 7) return QColor();
    return bright ? brightColors[code] : normal[code];
}

void AnsiTextEdit::applyAnsiSequence(const QString &seq, QTextCharFormat &fmt)
{
    // seq is the content between \033[ and m, e.g. "1;31"
    QStringList parts = seq.split(';');
    bool bright = false;
    for (const QString &part : parts)
    {
        int code = part.toInt();
        if (code == 0)
        {
            fmt = QTextCharFormat(); // reset
            bright = false;
        }
        else if (code == 1) { fmt.setFontWeight(QFont::Bold); bright = true; }
        else if (code == 3) { fmt.setFontItalic(true); }
        else if (code == 4) { fmt.setFontUnderline(true); }
        else if (code >= 30 && code <= 37)
        {
            fmt.setForeground(ansiToColor(code - 30, bright));
        }
        else if (code >= 40 && code <= 47)
        {
            fmt.setBackground(ansiToColor(code - 40, false));
        }
        else if (code >= 90 && code <= 97)
        {
            fmt.setForeground(ansiToColor(code - 90, true));
        }
    }
}

void AnsiTextEdit::appendAnsi(const QString &text)
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);

    static QRegularExpression re("\033\\[([0-9;]*)m");
    QTextCharFormat currentFmt;

    int lastEnd = 0;
    auto it = re.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        // Plain text before this escape
        if (match.capturedStart() > lastEnd)
        {
            cursor.insertText(text.mid(lastEnd, match.capturedStart() - lastEnd), currentFmt);
        }
        applyAnsiSequence(match.captured(1), currentFmt);
        lastEnd = match.capturedEnd();
    }
    // Remaining text after last escape
    if (lastEnd < text.length())
    {
        cursor.insertText(text.mid(lastEnd), currentFmt);
    }

    setTextCursor(cursor);
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

// ---- TerminalWidget implementation -----------------------------------------

class InputLineEdit : public QLineEdit
{
public:
    explicit InputLineEdit(TerminalWidget *term, QWidget *parent = nullptr)
        : QLineEdit(parent), m_terminal(term) {}

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Up)
        {
            emit m_terminal->metaObject()->invokeMethod(m_terminal, "onHistoryUp",
                Qt::DirectConnection);
            return;
        }
        if (event->key() == Qt::Key_Down)
        {
            emit m_terminal->metaObject()->invokeMethod(m_terminal, "onHistoryDown",
                Qt::DirectConnection);
            return;
        }
        QLineEdit::keyPressEvent(event);
    }

private:
    TerminalWidget *m_terminal;
};

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent), m_historyIndex(-1), m_processRunning(false)
{
    m_currentDir = QDir::currentPath();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    // Output pane
    m_output = new AnsiTextEdit(this);
    layout->addWidget(m_output, 1);

    // Search bar (hidden by default)
    m_searchBar = new QWidget(this);
    auto *searchLayout = new QHBoxLayout(m_searchBar);
    searchLayout->setContentsMargins(4, 2, 4, 2);
    m_searchInput = new QLineEdit;
    m_searchInput->setPlaceholderText("Search scrollback...");
    m_searchStatus = new QLabel;
    auto *nextBtn = new QPushButton("Next");
    auto *prevBtn = new QPushButton("Prev");
    auto *closeBtn = new QPushButton("×");
    closeBtn->setFixedWidth(24);
    searchLayout->addWidget(m_searchInput, 1);
    searchLayout->addWidget(m_searchStatus);
    searchLayout->addWidget(prevBtn);
    searchLayout->addWidget(nextBtn);
    searchLayout->addWidget(closeBtn);
    m_searchBar->hide();
    layout->addWidget(m_searchBar);

    connect(nextBtn, &QPushButton::clicked, this, &TerminalWidget::onSearchNext);
    connect(prevBtn, &QPushButton::clicked, this, &TerminalWidget::onSearchPrev);
    connect(closeBtn, &QPushButton::clicked, [this]() { m_searchBar->hide(); m_input->setFocus(); });
    connect(m_searchInput, &QLineEdit::returnPressed, this, &TerminalWidget::onSearchNext);

    // Input line
    auto *inputLayout = new QHBoxLayout;
    auto *promptLabel = new QLabel("shellx> ");
    promptLabel->setStyleSheet("font-weight: bold; color: #3B8EEA;");
    m_input = new QLineEdit;
    m_input->setPlaceholderText("Enter command...");
    inputLayout->addWidget(promptLabel);
    inputLayout->addWidget(m_input, 1);
    layout->addLayout(inputLayout);

    m_highlighter = new CommandHighlighter(m_input);

    connect(m_input, &QLineEdit::returnPressed, this, &TerminalWidget::onReturnPressed);

    // Ctrl+F for search
    auto *searchShortcut = new QShortcut(QKeySequence::Find, this);
    connect(searchShortcut, &QShortcut::activated, this, &TerminalWidget::onSearchToggled);

    // QProcess for external commands
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalWidget::onProcessOutput);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TerminalWidget::onProcessFinished);

    appendOutput(".........Welcome to ShellX........\n");
}

void TerminalWidget::focusInput()
{
    m_input->setFocus();
}

void TerminalWidget::onReturnPressed()
{
    QString cmd = m_input->text().trimmed();
    m_input->clear();

    if (cmd.isEmpty()) return;

    m_history.append(cmd);
    m_historyIndex = m_history.size();

    m_output->appendAnsi(QString("\033[1;34mshellx>\033[0m %1\n").arg(cmd));

    runCommand(cmd);
}

static QStringList parseCommandLine(const QString &input)
{
    QStringList tokens;
    QString current;
    bool inDouble = false, inSingle = false, escaped = false;

    for (int i = 0; i < input.length(); ++i)
    {
        QChar c = input[i];

        if (escaped)
        {
            current += c;
            escaped = false;
            continue;
        }

        if (c == '\\' && !inSingle)
        {
            escaped = true;
            continue;
        }

        if (c == '"' && !inSingle)  { inDouble = !inDouble; continue; }
        if (c == '\'' && !inDouble) { inSingle = !inSingle; continue; }

        if (c.isSpace() && !inDouble && !inSingle)
        {
            if (!current.isEmpty()) { tokens.append(current); current.clear(); }
            continue;
        }

        current += c;
    }
    if (!current.isEmpty()) tokens.append(current);
    return tokens;
}

void TerminalWidget::runCommand(const QString &cmd)
{
    QStringList parts = parseCommandLine(cmd);
    if (parts.isEmpty()) return;

    QString command = parts[0];
    QStringList args = parts.mid(1);

    if (command == "exit" || command == "quit")
    {
        appendOutput("Use Ctrl+W to close the tab or Ctrl+Q to exit.\n");
        return;
    }

    if (command == "cd" || command == "ls" || command == "pwd" || command == "clear")
    {
        executeBuiltin(command, args);
    }
    else
    {
        executeExternal(cmd);
    }
}

void TerminalWidget::executeBuiltin(const QString &cmd, const QStringList &args)
{
    if (cmd == "cd")
    {
        QString target;
        if (args.isEmpty())
            target = QDir::homePath();
        else
        {
            target = args.join(' ');
            if (target.startsWith("~"))
                target = QDir::homePath() + target.mid(1);
        }

        QDir dir(target);
        if (!dir.isAbsolute())
            dir = QDir(m_currentDir + "/" + target);

        if (dir.exists())
        {
            m_currentDir = dir.canonicalPath();
            m_process->setWorkingDirectory(m_currentDir);
            emit directoryChanged(m_currentDir);
            appendOutput(QString("Changed to: %1\n").arg(m_currentDir));
        }
        else
        {
            appendOutput(QString("\033[31mcd: no such directory: %1\033[0m\n").arg(target));
        }
    }
    else if (cmd == "ls")
    {
        QDir dir(m_currentDir);
        QStringList entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                             QDir::DirsFirst | QDir::Name);
        for (const QString &entry : entries)
        {
            QFileInfo info(m_currentDir + "/" + entry);
            if (info.isDir())
                m_output->appendAnsi(QString("\033[1;34m%1/\033[0m\n").arg(entry));
            else if (info.isExecutable())
                m_output->appendAnsi(QString("\033[1;32m%1\033[0m\n").arg(entry));
            else
                appendOutput(entry + "\n");
        }
    }
    else if (cmd == "pwd")
    {
        appendOutput(m_currentDir + "\n");
    }
    else if (cmd == "clear")
    {
        m_output->clear();
    }
}

void TerminalWidget::executeExternal(const QString &cmdLine)
{
    if (m_processRunning)
    {
        appendOutput("\033[33mA command is already running. Please wait.\033[0m\n");
        return;
    }

    m_processRunning = true;
    m_input->setEnabled(false);

    m_process->setWorkingDirectory(m_currentDir);
    m_process->start("/bin/sh", QStringList() << "-c" << cmdLine);
}

void TerminalWidget::onProcessOutput()
{
    QByteArray data = m_process->readAllStandardOutput();
    m_output->appendAnsi(QString::fromLocal8Bit(data));
}

void TerminalWidget::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    if (exitCode != 0)
    {
        appendOutput(QString("\033[33m[exit code: %1]\033[0m\n").arg(exitCode));
    }
    m_processRunning = false;
    m_input->setEnabled(true);
    m_input->setFocus();
}

void TerminalWidget::appendOutput(const QString &text)
{
    m_output->appendAnsi(text);
}

// ---- Search in scrollback --------------------------------------------------

void TerminalWidget::onSearchToggled()
{
    if (m_searchBar->isVisible())
    {
        m_searchBar->hide();
        m_input->setFocus();
    }
    else
    {
        m_searchBar->show();
        m_searchInput->setFocus();
        m_searchInput->selectAll();
    }
}

void TerminalWidget::onSearchNext()
{
    QString query = m_searchInput->text();
    if (query.isEmpty()) return;

    bool found = m_output->find(query);
    if (!found)
    {
        // Wrap around from the beginning
        QTextCursor cursor = m_output->textCursor();
        cursor.movePosition(QTextCursor::Start);
        m_output->setTextCursor(cursor);
        found = m_output->find(query);
    }
    m_searchStatus->setText(found ? "" : "Not found");
}

void TerminalWidget::onSearchPrev()
{
    QString query = m_searchInput->text();
    if (query.isEmpty()) return;

    bool found = m_output->find(query, QTextDocument::FindBackward);
    if (!found)
    {
        QTextCursor cursor = m_output->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_output->setTextCursor(cursor);
        found = m_output->find(query, QTextDocument::FindBackward);
    }
    m_searchStatus->setText(found ? "" : "Not found");
}
