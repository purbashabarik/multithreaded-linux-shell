// ============================================================================
// File:    CommandPalette.h
// Summary: A VS Code-style command palette dialog (Ctrl+Shift+P).
//          Features:
//          - Fuzzy search over built-in commands, command history, and files
//            in the current working directory.
//          - Results ranked by subsequence match quality with bonus for
//            consecutive character matches.
//          - Keyboard navigation: arrow keys + Enter to select, Esc to close.
//          - Grouped results with headers (Commands, History, Files).
// ============================================================================

#ifndef EDS_COMMAND_PALETTE_H
#define EDS_COMMAND_PALETTE_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QStringList>

class CommandPalette : public QDialog
{
    Q_OBJECT

public:
    explicit CommandPalette(const QStringList &history, QWidget *parent = nullptr);

    QString selectedCommand() const { return m_selected; }

private slots:
    void onTextChanged(const QString &text);
    void onItemActivated(QListWidgetItem *item);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    struct ScoredItem {
        QString text;
        QString category;
        int score;
    };

    int fuzzyScore(const QString &pattern, const QString &candidate) const;
    void populateResults(const QString &filter);

    QLineEdit *m_searchInput;
    QListWidget *m_resultsList;
    QStringList m_history;
    QStringList m_builtins;
    QString m_selected;
};

#endif // EDS_COMMAND_PALETTE_H
