// ============================================================================
// File:    CommandPalette.cpp
// Summary: Implementation of the command palette. Collects candidates from
//          three sources (built-in commands, command history, current dir
//          files), scores them using subsequence fuzzy matching, sorts by
//          score descending, and displays grouped results. The fuzzy score
//          rewards consecutive character matches and start-of-word matches.
// ============================================================================

#include "CommandPalette.h"
#include <QVBoxLayout>
#include <QDir>
#include <QKeyEvent>
#include <QFont>
#include <algorithm>

CommandPalette::CommandPalette(const QStringList &history, QWidget *parent)
    : QDialog(parent), m_history(history)
{
    setWindowTitle("Command Palette");
    setMinimumSize(500, 400);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    m_builtins = {"cd", "ls", "pwd", "clear", "exit", "mv", "cp", "rm",
                  "mkdir", "echo", "cat", "grep", "find", "head", "tail",
                  "sort", "wc", "chmod", "chown"};

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    m_searchInput = new QLineEdit;
    m_searchInput->setPlaceholderText("Type to search commands, history, files...");
    QFont inputFont = m_searchInput->font();
    inputFont.setPointSize(14);
    m_searchInput->setFont(inputFont);
    layout->addWidget(m_searchInput);

    m_resultsList = new QListWidget;
    m_resultsList->setAlternatingRowColors(true);
    layout->addWidget(m_resultsList, 1);

    connect(m_searchInput, &QLineEdit::textChanged, this, &CommandPalette::onTextChanged);
    connect(m_resultsList, &QListWidget::itemActivated, this, &CommandPalette::onItemActivated);
    connect(m_resultsList, &QListWidget::itemDoubleClicked, this, &CommandPalette::onItemActivated);

    m_searchInput->installEventFilter(this);

    populateResults("");
    m_searchInput->setFocus();
}

int CommandPalette::fuzzyScore(const QString &pattern, const QString &candidate) const
{
    if (pattern.isEmpty()) return 1;

    QString lowerPattern = pattern.toLower();
    QString lowerCandidate = candidate.toLower();

    int score = 0;
    int pi = 0;
    int consecutive = 0;

    for (int ci = 0; ci < lowerCandidate.length() && pi < lowerPattern.length(); ++ci)
    {
        if (lowerCandidate[ci] == lowerPattern[pi])
        {
            score += 10;
            consecutive++;
            score += consecutive * 5; // bonus for consecutive matches
            if (ci == 0) score += 20; // bonus for matching at start

            // Bonus for matching after separator (space, /, -, _)
            if (ci > 0)
            {
                QChar prev = lowerCandidate[ci - 1];
                if (prev == ' ' || prev == '/' || prev == '-' || prev == '_')
                    score += 15;
            }
            pi++;
        }
        else
        {
            consecutive = 0;
        }
    }

    if (pi < lowerPattern.length()) return -1; // pattern not fully matched

    return score;
}

void CommandPalette::populateResults(const QString &filter)
{
    m_resultsList->clear();

    QVector<ScoredItem> items;

    // Built-in commands
    for (const QString &cmd : m_builtins)
    {
        int s = fuzzyScore(filter, cmd);
        if (s >= 0) items.append({cmd, "Command", s});
    }

    // History (deduplicated, most recent first)
    QSet<QString> seen;
    for (int i = m_history.size() - 1; i >= 0; --i)
    {
        const QString &h = m_history[i];
        if (seen.contains(h)) continue;
        seen.insert(h);
        int s = fuzzyScore(filter, h);
        if (s >= 0) items.append({h, "History", s + 2}); // slight boost for history
    }

    // Files in current directory
    QDir dir = QDir::current();
    QStringList files = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QString &f : files)
    {
        int s = fuzzyScore(filter, f);
        if (s >= 0) items.append({f, "File", s});
    }

    // Sort by score descending
    std::sort(items.begin(), items.end(), [](const ScoredItem &a, const ScoredItem &b) {
        return a.score > b.score;
    });

    // Display with category headers
    QString lastCat;
    int count = 0;
    for (const ScoredItem &item : items)
    {
        if (count >= 50) break;

        if (item.category != lastCat)
        {
            auto *header = new QListWidgetItem(QString("── %1 ──").arg(item.category));
            header->setFlags(Qt::NoItemFlags);
            QFont f = header->font();
            f.setBold(true);
            f.setPointSize(10);
            header->setFont(f);
            header->setForeground(QColor(120, 120, 120));
            m_resultsList->addItem(header);
            lastCat = item.category;
        }

        auto *listItem = new QListWidgetItem(QString("  %1").arg(item.text));
        listItem->setData(Qt::UserRole, item.text);
        m_resultsList->addItem(listItem);
        count++;
    }

    if (m_resultsList->count() > 0)
    {
        // Select first selectable item
        for (int i = 0; i < m_resultsList->count(); ++i)
        {
            if (m_resultsList->item(i)->flags() & Qt::ItemIsSelectable)
            {
                m_resultsList->setCurrentRow(i);
                break;
            }
        }
    }
}

void CommandPalette::onTextChanged(const QString &text)
{
    populateResults(text);
}

void CommandPalette::onItemActivated(QListWidgetItem *item)
{
    if (!item || !(item->flags() & Qt::ItemIsSelectable)) return;
    m_selected = item->data(Qt::UserRole).toString();
    accept();
}

bool CommandPalette::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_searchInput && event->type() == QEvent::KeyPress)
    {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Down)
        {
            int row = m_resultsList->currentRow();
            for (int i = row + 1; i < m_resultsList->count(); ++i)
            {
                if (m_resultsList->item(i)->flags() & Qt::ItemIsSelectable)
                {
                    m_resultsList->setCurrentRow(i);
                    break;
                }
            }
            return true;
        }
        if (keyEvent->key() == Qt::Key_Up)
        {
            int row = m_resultsList->currentRow();
            for (int i = row - 1; i >= 0; --i)
            {
                if (m_resultsList->item(i)->flags() & Qt::ItemIsSelectable)
                {
                    m_resultsList->setCurrentRow(i);
                    break;
                }
            }
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
        {
            auto *current = m_resultsList->currentItem();
            if (current && (current->flags() & Qt::ItemIsSelectable))
            {
                onItemActivated(current);
            }
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape)
        {
            reject();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}
