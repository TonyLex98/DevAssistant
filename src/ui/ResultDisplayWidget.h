#pragma once
#include <QWidget>
#include <QSet>
#include <QString>
#include <QTreeWidget>

#include "../core/ContextManager.h"

class QScrollArea;
class QVBoxLayout;

class ResultDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    enum class EntryType {
        Default,
        Reasoning,
        UserQuestion
    };

    struct ResultEntryMetadata {
        QList<CommandContextInfo> commands;
        QList<CommandSequenceInfo> sequences;
    };

    explicit ResultDisplayWidget(QWidget *parent = nullptr);

    void appendResult(const QString &title,
                      const QString &content,
                      EntryType type = EntryType::Default,
                      const ResultEntryMetadata &metadata = ResultEntryMetadata());

private:
    void convertMarkdownToHtml(QString &htmlContent);
    QWidget *createEntryWidget(const QString &title,
                               const QString &htmlContent,
                               const QString &sourceText,
                               EntryType type,
                               const ResultEntryMetadata &metadata);
    void populateContextTree(QTreeWidget *tree, const ResultEntryMetadata &metadata);
    void addCommandItem(QTreeWidget *tree,
                        QTreeWidgetItem *parent,
                        const CommandContextInfo &cmd,
                        QSet<QString> &addedCommands);
    QString reasoningSnippet(const QString &source) const;
    void scrollToBottom();

    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_contentWidget = nullptr;
    QVBoxLayout *m_layout = nullptr;
};
