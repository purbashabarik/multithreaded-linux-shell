// ============================================================================
// File:    CommandHighlighter.h
// Summary: Live syntax highlighter for the command input QLineEdit.
//          Highlights as the user types:
//            - Commands: green if known built-in or found in $PATH, red if not
//            - Quoted strings: orange
//            - Flags (-x, --xxx): cyan
//            - Pipe/redirect operators (|, <, >, >>): yellow
//          Works by connecting to textChanged() on the QLineEdit and
//          applying QTextCharFormat coloring to the underlying QTextDocument
//          of a helper QPlainTextEdit that overlays the input (Qt quirk:
//          QLineEdit doesn't natively support syntax highlighting, so we
//          use a single-line QPlainTextEdit as a workaround).
//          Alternatively, this paints directly using QPainter overlay.
// ============================================================================

#ifndef EDS_COMMAND_HIGHLIGHTER_H
#define EDS_COMMAND_HIGHLIGHTER_H

#include <QObject>
#include <QLineEdit>
#include <QSet>

class CommandHighlighter : public QObject
{
    Q_OBJECT

public:
    explicit CommandHighlighter(QLineEdit *input, QObject *parent = nullptr);

private slots:
    void onTextChanged(const QString &text);

private:
    bool isBuiltinCommand(const QString &cmd) const;
    bool isExternalCommand(const QString &cmd) const;
    QString colorize(const QString &text, const QString &color) const;

    QLineEdit *m_input;
    QSet<QString> m_builtins;
    QStringList m_pathDirs;
    QString m_lastStyled;
};

#endif // EDS_COMMAND_HIGHLIGHTER_H
