#pragma once

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

struct CommandContextInfo;

// Graph node for hierarchical context memory.
struct ContextGraphNodeInfo {
    QString id;
    QString parentId;
    QString category;
    QString title;
    QString summary;
    QDateTime createdAt;
    QDateTime updatedAt;
    QStringList characteristicTags;
    QStringList actions;
    bool milestone = false;
};

// What the conversation has been about, as a tree: one root per category (project, news,
// search, system, general), threads of user queries below it, and the commands run for each
// thread as its actions. The model sees only the branch it is on, so an old thread does not
// crowd the prompt; the UI shows the whole tree.
class ContextGraph
{
public:
    // Remembers filePath so that every later change can be saved there.
    void load(const QString& filePath);
    void save() const;
    void clear();

    // Roots first, then breadth-first, so a parent always comes before its children.
    QList<ContextGraphNodeInfo> nodes() const;
    QString activeNodeId() const { return m_lastActiveNodeId; }

    // A query close to the active thread continues it; a different one starts a branch.
    void advanceByUserQuery(const QString& text);
    void attachCommand(const CommandContextInfo& info);

    QString contextText(int maxNodes, int maxActionsPerNode) const;

private:
    QString inferCategoryForCommand(const CommandContextInfo& info) const;
    QString ensureCategoryRootNode(const QString& category);
    QString createNode(const QString& parentId,
                       const QString& category,
                       const QString& title,
                       const QString& summary,
                       const QStringList& tags,
                       bool milestone = false);
    void addActionToNode(const QString& nodeId, const QString& action, bool milestone = false);
    void prune();

    QMap<QString, ContextGraphNodeInfo> m_nodes;
    QMap<QString, QString> m_categoryRootNodeIds;
    QMap<QString, QString> m_activeNodeByCategory;
    QString m_lastActiveNodeId;
    QString m_filePath;
    int m_nodeCounter = 0;
};
