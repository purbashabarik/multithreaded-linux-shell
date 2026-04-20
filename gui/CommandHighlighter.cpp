// ============================================================================
// File:    CommandHighlighter.cpp
// Summary: Implements live syntax highlighting for command input.
//          Since QLineEdit doesn't support QSyntaxHighlighter, we use a
//          stylesheet-based approach: the entire input text color is set
//          based on whether the first token is a valid command. For richer
//          highlighting, the input QLineEdit could be replaced with a
//          single-line QPlainTextEdit + QSyntaxHighlighter in a future
//          iteration. The current approach keeps things simple and fast.
// ============================================================================

#include "CommandHighlighter.h"
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>

CommandHighlighter::CommandHighlighter(QLineEdit *input, QObject *parent)
    : QObject(parent ? parent : input), m_input(input)
{
    m_builtins = {"cd", "ls", "pwd", "clear", "exit", "quit", "mv", "cp", "rm", "mkdir"};

    QString pathEnv = QProcessEnvironment::systemEnvironment().value("PATH");
    m_pathDirs = pathEnv.split(':');

    connect(m_input, &QLineEdit::textChanged, this, &CommandHighlighter::onTextChanged);
}

bool CommandHighlighter::isBuiltinCommand(const QString &cmd) const
{
    return m_builtins.contains(cmd);
}

bool CommandHighlighter::isExternalCommand(const QString &cmd) const
{
    if (cmd.contains('/'))
        return QFileInfo(cmd).isExecutable();

    for (const QString &dir : m_pathDirs)
    {
        QFileInfo fi(dir + "/" + cmd);
        if (fi.exists() && fi.isExecutable())
            return true;
    }
    return false;
}

void CommandHighlighter::onTextChanged(const QString &text)
{
    if (text.isEmpty())
    {
        m_input->setStyleSheet("");
        return;
    }

    QStringList parts = text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;

    QString firstToken = parts[0];

    // Determine color for the command token
    QString style;
    if (isBuiltinCommand(firstToken))
    {
        style = "color: #23D18B; font-weight: bold;"; // green for built-ins
    }
    else if (isExternalCommand(firstToken))
    {
        style = "color: #3B8EEA; font-weight: bold;"; // blue for external
    }
    else if (firstToken.length() >= 2)
    {
        style = "color: #F14C4C;"; // red for unknown
    }
    else
    {
        style = ""; // typing in progress
    }

    if (style != m_lastStyled)
    {
        m_input->setStyleSheet(style.isEmpty() ? "" : QString("QLineEdit { %1 }").arg(style));
        m_lastStyled = style;
    }
}
