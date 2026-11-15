#include "core/context/ContextGraph.h"
#include "core/context/ContextKeywords.h"
#include "core/context/ContextManager.h"
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQueue>
#include <QSaveFile>
#include <QSet>
#include <algorithm>
#include "core/util/Logging.h"

namespace {
QString normalizeCategory(const QString& category)
{
    const QString c = category.trimmed().toLower();
    if (c == QStringLiteral("project") || c == QStringLiteral("coding"))
        return QStringLiteral("project");
    if (c == QStringLiteral("news"))
        return QStringLiteral("news");
    if (c == QStringLiteral("search"))
        return QStringLiteral("search");
    if (c == QStringLiteral("system"))
        return QStringLiteral("system");
    return QStringLiteral("general");
}

QString defaultCategoryTitle(const QString& category)
{
    const QString c = normalizeCategory(category);
    if (c == QStringLiteral("project"))
        return QStringLiteral("Project and Coding");
    if (c == QStringLiteral("news"))
        return QStringLiteral("News and Events");
    if (c == QStringLiteral("search"))
        return QStringLiteral("Exploration and Search");
    if (c == QStringLiteral("system"))
        return QStringLiteral("System and Tooling");
    return QStringLiteral("General Context");
}

QString shortText(const QString& text, int maxChars)
{
    QString t = text.simplified();
    if (maxChars > 0 && t.size() > maxChars)
        t = t.left(maxChars) + QStringLiteral("...");
    return t;
}

QString inferCategoryFromText(const QString& text)
{
    const QString lower = text.toLower();
    const bool looksLikeNews =
        lower.contains(QStringLiteral("\u043d\u043e\u0432\u043e\u0441\u0442")) ||
        lower.contains(QStringLiteral("\u0443\u043a\u0440\u0430\u0438\u043d")) ||
        lower.contains(QStringLiteral("\u0440\u043e\u0441\u0441\u0438")) ||
        lower.contains(QStringLiteral("latest")) ||
        lower.contains(QStringLiteral("breaking")) ||
        lower.contains(QStringLiteral("news"));
    if (looksLikeNews)
        return QStringLiteral("news");

    const bool looksLikeProject =
        lower.contains(QStringLiteral("\u043f\u0440\u043e\u0435\u043a\u0442")) ||
        lower.contains(QStringLiteral("\u043a\u043e\u0434")) ||
        lower.contains(QStringLiteral("\u0444\u0430\u0439\u043b")) ||
        lower.contains(QStringLiteral("\u043e\u0448\u0438\u0431\u043a")) ||
        lower.contains(QStringLiteral("bug")) ||
        lower.contains(QStringLiteral("refactor")) ||
        lower.contains(QStringLiteral("compile")) ||
        lower.contains(QStringLiteral("qt")) ||
        lower.contains(QStringLiteral("c++")) ||
        lower.contains(QStringLiteral("cpp")) ||
        lower.contains(QStringLiteral("python")) ||
        lower.contains(QStringLiteral("java"));
    if (looksLikeProject)
        return QStringLiteral("project");

    const bool looksLikeSystem =
        lower.contains(QStringLiteral("powershell")) ||
        lower.contains(QStringLiteral("terminal")) ||
        lower.contains(QStringLiteral("shell")) ||
        lower.contains(QStringLiteral("bash")) ||
        lower.contains(QStringLiteral("cmd")) ||
        lower.contains(QStringLiteral("git"));
    if (looksLikeSystem)
        return QStringLiteral("system");

    const bool looksLikeSearch =
        lower.contains(QStringLiteral("\u043d\u0430\u0439\u0434")) ||
        lower.contains(QStringLiteral("\u043f\u043e\u0438\u0441\u043a")) ||
        lower.contains(QStringLiteral("search")) ||
        lower.contains(QStringLiteral("google")) ||
        lower.contains(QStringLiteral("web"));
    if (looksLikeSearch)
        return QStringLiteral("search");

    return QStringLiteral("general");
}
} // namespace

QList<ContextGraphNodeInfo> ContextGraph::nodes() const
{
    QList<ContextGraphNodeInfo> ordered;
    if (m_nodes.isEmpty())
        return ordered;

    QSet<QString> visited;
    QQueue<QString> queue;
    QStringList roots = m_categoryRootNodeIds.values();
    roots.removeDuplicates();
    std::sort(roots.begin(), roots.end());
    for (const QString& rootId : roots) {
        if (m_nodes.contains(rootId))
            queue.enqueue(rootId);
    }

    while (!queue.isEmpty()) {
        const QString id = queue.dequeue();
        if (visited.contains(id) || !m_nodes.contains(id))
            continue;
        visited.insert(id);
        ordered << m_nodes.value(id);

        QList<ContextGraphNodeInfo> children;
        for (const auto& node : m_nodes) {
            if (node.parentId == id)
                children << node;
        }
        std::sort(children.begin(), children.end(), [](const ContextGraphNodeInfo& a, const ContextGraphNodeInfo& b) {
            if (a.updatedAt.isValid() && b.updatedAt.isValid() && a.updatedAt != b.updatedAt)
                return a.updatedAt < b.updatedAt;
            return a.id < b.id;
        });
        for (const auto& child : children)
            queue.enqueue(child.id);
    }

    QList<ContextGraphNodeInfo> remaining;
    for (auto it = m_nodes.cbegin(); it != m_nodes.cend(); ++it) {
        if (!visited.contains(it.key()))
            remaining << it.value();
    }
    std::sort(remaining.begin(), remaining.end(), [](const ContextGraphNodeInfo& a, const ContextGraphNodeInfo& b) {
        if (a.updatedAt.isValid() && b.updatedAt.isValid() && a.updatedAt != b.updatedAt)
            return a.updatedAt < b.updatedAt;
        return a.id < b.id;
    });
    ordered.append(remaining);
    return ordered;
}

QString ContextGraph::inferCategoryForCommand(const CommandContextInfo& info) const
{
    const QString name = info.name.trimmed().toUpper();
    if (name == QStringLiteral("SEARCH_WEB")) {
        const QString q = info.params.value(QStringLiteral("query")).toString();
        const QString byQuery = inferCategoryFromText(q);
        if (byQuery == QStringLiteral("general"))
            return QStringLiteral("search");
        return byQuery;
    }

    if (name.startsWith(QStringLiteral("SEARCH_PROJECT")) ||
        name.startsWith(QStringLiteral("OPEN_FILE")) ||
        name == QStringLiteral("CREATE_FILE") ||
        name == QStringLiteral("REPLACE_IN_FILE") ||
        name == QStringLiteral("EDIT_IN_RANGE") ||
        name == QStringLiteral("APPEND_TO_FILE") ||
        name == QStringLiteral("INSERT_AT_LINE") ||
        name == QStringLiteral("APPLY_PATCH")) {
        return QStringLiteral("project");
    }

    if (name.startsWith(QStringLiteral("GIT_")) ||
        name == QStringLiteral("RUN_SHELL") ||
        name == QStringLiteral("RUN_PYTHON")) {
        return QStringLiteral("system");
    }

    if (name == QStringLiteral("SHOW_RESULT") || name == QStringLiteral("REASONING")) {
        if (!m_lastActiveNodeId.isEmpty() && m_nodes.contains(m_lastActiveNodeId))
            return normalizeCategory(m_nodes.value(m_lastActiveNodeId).category);
        return QStringLiteral("general");
    }

    const QString fromText = inferCategoryFromText(info.description + QStringLiteral(" ") + info.result);
    return normalizeCategory(fromText);
}

QString ContextGraph::ensureCategoryRootNode(const QString& category)
{
    const QString normalizedCategory = normalizeCategory(category);
    const QString existing = m_categoryRootNodeIds.value(normalizedCategory);
    if (!existing.isEmpty() && m_nodes.contains(existing))
        return existing;

    const QString id = QStringLiteral("CAT_%1").arg(normalizedCategory);
    ContextGraphNodeInfo root;
    root.id = id;
    root.parentId.clear();
    root.category = normalizedCategory;
    root.title = defaultCategoryTitle(normalizedCategory);
    root.summary = QStringLiteral("Root branch for %1").arg(normalizedCategory);
    root.createdAt = QDateTime::currentDateTime();
    root.updatedAt = root.createdAt;
    root.characteristicTags = QStringList{normalizedCategory};
    root.milestone = true;

    m_nodes[id] = root;
    m_categoryRootNodeIds[normalizedCategory] = id;
    return id;
}

QString ContextGraph::createNode(const QString& parentId,
                                 const QString& category,
                                 const QString& title,
                                 const QString& summary,
                                 const QStringList& tags,
                                 bool milestone)
{
    const QString normalizedCategory = normalizeCategory(category);
    const QString id = QStringLiteral("CGN_%1").arg(++m_nodeCounter);

    ContextGraphNodeInfo node;
    node.id = id;
    node.parentId = parentId;
    node.category = normalizedCategory;
    node.title = shortText(title, 72);
    node.summary = shortText(summary, 200);
    node.createdAt = QDateTime::currentDateTime();
    node.updatedAt = node.createdAt;
    node.characteristicTags = tags;
    node.characteristicTags.removeDuplicates();
    node.milestone = milestone;

    m_nodes[id] = node;
    prune();
    return id;
}

void ContextGraph::addActionToNode(const QString& nodeId, const QString& action, bool milestone)
{
    if (nodeId.isEmpty() || !m_nodes.contains(nodeId))
        return;

    ContextGraphNodeInfo node = m_nodes.value(nodeId);
    const QString compactAction = shortText(action, 260);
    if (compactAction.isEmpty())
        return;

    node.actions << compactAction;
    while (node.actions.size() > 24)
        node.actions.removeFirst();
    node.updatedAt = QDateTime::currentDateTime();
    if (milestone)
        node.milestone = true;

    if (node.actions.size() > 12) {
        const QString movedAction = node.actions.takeLast();
        m_nodes[nodeId] = node;

        const QString childId = createNode(
            nodeId,
            node.category,
            QStringLiteral("%1 / branch").arg(shortText(node.title, 48)),
            QStringLiteral("Auto-created continuation branch"),
            node.characteristicTags,
            milestone);

        ContextGraphNodeInfo child = m_nodes.value(childId);
        child.actions << movedAction;
        child.updatedAt = QDateTime::currentDateTime();
        child.milestone = child.milestone || milestone;
        m_nodes[childId] = child;

        m_activeNodeByCategory[node.category] = childId;
        m_lastActiveNodeId = childId;
    } else {
        m_nodes[nodeId] = node;
        m_activeNodeByCategory[node.category] = nodeId;
        m_lastActiveNodeId = nodeId;
    }

    prune();
    save();
}

void ContextGraph::advanceByUserQuery(const QString& text)
{
    const QString category = inferCategoryFromText(text);
    const QString rootId = ensureCategoryRootNode(category);
    QString activeId = m_activeNodeByCategory.value(category);
    if (activeId.isEmpty() || !m_nodes.contains(activeId))
        activeId = rootId;

    const QSet<QString> queryKeywords = ContextKeywords::extract(text);
    const QStringList tags = queryKeywords.values();

    if (activeId == rootId) {
        activeId = createNode(rootId, category, shortText(text, 72), shortText(text, 200), tags, false);
    } else {
        ContextGraphNodeInfo active = m_nodes.value(activeId);
        QSet<QString> nodeKeywords;
        for (const QString& tag : active.characteristicTags)
            nodeKeywords.insert(tag.toLower());
        const double sim = ContextKeywords::jaccard(queryKeywords, nodeKeywords);
        if (sim < 0.25) {
            activeId = createNode(active.id, category, shortText(text, 72), shortText(text, 200), tags, false);
        } else {
            const QString queryText = shortText(text, 120);
            if (!active.summary.contains(queryText))
                active.summary = shortText(active.summary + QStringLiteral(" | ") + queryText, 240);
            active.updatedAt = QDateTime::currentDateTime();
            for (const QString& t : tags) {
                if (!active.characteristicTags.contains(t))
                    active.characteristicTags << t;
            }
            while (active.characteristicTags.size() > 20)
                active.characteristicTags.removeFirst();
            m_nodes[active.id] = active;
        }
    }

    m_activeNodeByCategory[category] = activeId;
    m_lastActiveNodeId = activeId;
    prune();
    save();
}

void ContextGraph::attachCommand(const CommandContextInfo& info)
{
    const QString category = inferCategoryForCommand(info);
    const QString rootId = ensureCategoryRootNode(category);
    QString activeId = m_activeNodeByCategory.value(category);
    if (activeId.isEmpty() || !m_nodes.contains(activeId))
        activeId = createNode(rootId,
                              category,
                              QStringLiteral("%1 thread").arg(defaultCategoryTitle(category)),
                              shortText(info.description, 180),
                              ContextKeywords::extract(info.description).values(),
                              false);

    QString action = QStringLiteral("%1: %2")
                         .arg(info.name,
                              shortText(info.description.isEmpty() ? info.name : info.description, 140));
    if (!info.result.trimmed().isEmpty()) {
        const QStringList lines = info.result.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty())
            action += QStringLiteral(" -> %1").arg(shortText(lines.first(), 140));
    }

    const bool milestone =
        info.name == QStringLiteral("SHOW_RESULT") ||
        info.name == QStringLiteral("APPLY_PATCH") ||
        info.name == QStringLiteral("EDIT_IN_RANGE") ||
        info.name == QStringLiteral("REPLACE_IN_FILE");

    addActionToNode(activeId, action, milestone);
    QString finalNodeId = m_activeNodeByCategory.value(category);
    if (finalNodeId.isEmpty())
        finalNodeId = activeId;
    m_activeNodeByCategory[category] = finalNodeId;
    if (!finalNodeId.isEmpty())
        m_lastActiveNodeId = finalNodeId;
}

QString ContextGraph::contextText(int maxNodes, int maxActionsPerNode) const
{
    if (m_nodes.isEmpty())
        return QString();

    QString activeId = m_lastActiveNodeId;
    if (activeId.isEmpty() || !m_nodes.contains(activeId)) {
        for (auto it = m_activeNodeByCategory.cbegin(); it != m_activeNodeByCategory.cend(); ++it) {
            if (m_nodes.contains(it.value())) {
                activeId = it.value();
                break;
            }
        }
    }
    if (activeId.isEmpty() || !m_nodes.contains(activeId))
        return QString();

    QStringList chainIds;
    QSet<QString> guard;
    QString cursor = activeId;
    while (!cursor.isEmpty() && m_nodes.contains(cursor) && !guard.contains(cursor)) {
        chainIds << cursor;
        guard.insert(cursor);
        cursor = m_nodes.value(cursor).parentId;
    }
    std::reverse(chainIds.begin(), chainIds.end());
    if (maxNodes > 0 && chainIds.size() > maxNodes)
        chainIds = chainIds.mid(chainIds.size() - maxNodes);

    QStringList lines;
    lines << QStringLiteral("=== CONTEXT GRAPH ===");
    lines << QStringLiteral("Active node: %1 [%2]")
                 .arg(m_nodes.value(activeId).title,
                      normalizeCategory(m_nodes.value(activeId).category));
    lines << QStringLiteral("Branch:");

    for (const QString& id : chainIds) {
        if (!m_nodes.contains(id))
            continue;
        const ContextGraphNodeInfo node = m_nodes.value(id);
        lines << QStringLiteral("- %1 :: %2").arg(node.title, shortText(node.summary, 180));

        if (!node.actions.isEmpty()) {
            lines << QStringLiteral("  Actions:");
            const int total = node.actions.size();
            const int from = qMax(0, total - qMax(1, maxActionsPerNode));
            for (int i = from; i < total; ++i)
                lines << QStringLiteral("  * %1").arg(node.actions.at(i));
        }
    }

    return lines.join(QStringLiteral("\n"));
}

void ContextGraph::prune()
{
    constexpr int kMaxGraphNodes = 260;
    if (m_nodes.size() <= kMaxGraphNodes)
        return;

    QSet<QString> protectedIds;
    for (const QString& rootId : m_categoryRootNodeIds.values())
        protectedIds.insert(rootId);

    for (const QString& activeId : m_activeNodeByCategory.values()) {
        QString cursor = activeId;
        QSet<QString> guard;
        while (!cursor.isEmpty() && m_nodes.contains(cursor) && !guard.contains(cursor)) {
            protectedIds.insert(cursor);
            guard.insert(cursor);
            cursor = m_nodes.value(cursor).parentId;
        }
    }

    while (m_nodes.size() > kMaxGraphNodes) {
        QHash<QString, int> childrenCount;
        for (auto it = m_nodes.cbegin(); it != m_nodes.cend(); ++it) {
            const QString parent = it.value().parentId;
            if (!parent.isEmpty())
                childrenCount[parent] += 1;
        }

        QString candidateId;
        QDateTime oldest;
        for (auto it = m_nodes.cbegin(); it != m_nodes.cend(); ++it) {
            const QString id = it.key();
            if (protectedIds.contains(id))
                continue;
            if (childrenCount.value(id) > 0)
                continue;

            const QDateTime ts = it.value().updatedAt.isValid() ? it.value().updatedAt : it.value().createdAt;
            if (candidateId.isEmpty() || (ts.isValid() && (!oldest.isValid() || ts < oldest))) {
                candidateId = id;
                oldest = ts;
            }
        }

        if (candidateId.isEmpty())
            break;

        m_nodes.remove(candidateId);
        for (auto it = m_activeNodeByCategory.begin(); it != m_activeNodeByCategory.end(); ++it) {
            if (it.value() == candidateId)
                it.value().clear();
        }
        if (m_lastActiveNodeId == candidateId)
            m_lastActiveNodeId.clear();
    }
}

void ContextGraph::load(const QString& filePath)
{
    m_filePath = filePath;
    m_nodes.clear();
    m_categoryRootNodeIds.clear();
    m_activeNodeByCategory.clear();
    m_lastActiveNodeId.clear();
    m_nodeCounter = 0;

    QFile f(m_filePath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ensureCategoryRootNode(QStringLiteral("general"));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) {
        ensureCategoryRootNode(QStringLiteral("general"));
        return;
    }

    const QJsonObject root = doc.object();
    m_nodeCounter = root.value(QStringLiteral("counter")).toInt(0);
    m_lastActiveNodeId = root.value(QStringLiteral("lastActive")).toString();

    const QJsonArray nodes = root.value(QStringLiteral("nodes")).toArray();
    for (const QJsonValue& v : nodes) {
        const QJsonObject o = v.toObject();
        ContextGraphNodeInfo node;
        node.id = o.value(QStringLiteral("id")).toString();
        node.parentId = o.value(QStringLiteral("parentId")).toString();
        node.category = normalizeCategory(o.value(QStringLiteral("category")).toString());
        node.title = o.value(QStringLiteral("title")).toString();
        node.summary = o.value(QStringLiteral("summary")).toString();
        node.createdAt = QDateTime::fromString(o.value(QStringLiteral("createdAt")).toString(), Qt::ISODate);
        node.updatedAt = QDateTime::fromString(o.value(QStringLiteral("updatedAt")).toString(), Qt::ISODate);
        node.milestone = o.value(QStringLiteral("milestone")).toBool(false);

        for (const QJsonValue& tv : o.value(QStringLiteral("tags")).toArray())
            node.characteristicTags << tv.toString();
        for (const QJsonValue& av : o.value(QStringLiteral("actions")).toArray())
            node.actions << av.toString();

        if (!node.id.isEmpty())
            m_nodes.insert(node.id, node);
    }

    const QJsonObject roots = root.value(QStringLiteral("roots")).toObject();
    for (auto it = roots.begin(); it != roots.end(); ++it)
        m_categoryRootNodeIds[normalizeCategory(it.key())] = it.value().toString();

    const QJsonObject activeByCategory = root.value(QStringLiteral("activeByCategory")).toObject();
    for (auto it = activeByCategory.begin(); it != activeByCategory.end(); ++it)
        m_activeNodeByCategory[normalizeCategory(it.key())] = it.value().toString();

    ensureCategoryRootNode(QStringLiteral("general"));
    prune();
}

void ContextGraph::save() const
{
    if (m_filePath.isEmpty())
        return;

    QJsonArray nodes;
    for (const auto& node : m_nodes) {
        QJsonObject o;
        o[QStringLiteral("id")] = node.id;
        o[QStringLiteral("parentId")] = node.parentId;
        o[QStringLiteral("category")] = normalizeCategory(node.category);
        o[QStringLiteral("title")] = node.title;
        o[QStringLiteral("summary")] = node.summary;
        o[QStringLiteral("createdAt")] = node.createdAt.isValid() ? node.createdAt.toString(Qt::ISODate) : QString();
        o[QStringLiteral("updatedAt")] = node.updatedAt.isValid() ? node.updatedAt.toString(Qt::ISODate) : QString();
        o[QStringLiteral("milestone")] = node.milestone;

        QJsonArray tags;
        for (const QString& tag : node.characteristicTags)
            tags.append(tag);
        o[QStringLiteral("tags")] = tags;

        QJsonArray actions;
        for (const QString& action : node.actions)
            actions.append(action);
        o[QStringLiteral("actions")] = actions;
        nodes.append(o);
    }

    QJsonObject roots;
    for (auto it = m_categoryRootNodeIds.cbegin(); it != m_categoryRootNodeIds.cend(); ++it)
        roots[it.key()] = it.value();

    QJsonObject activeByCategory;
    for (auto it = m_activeNodeByCategory.cbegin(); it != m_activeNodeByCategory.cend(); ++it)
        activeByCategory[it.key()] = it.value();

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("counter")] = m_nodeCounter;
    root[QStringLiteral("lastActive")] = m_lastActiveNodeId;
    root[QStringLiteral("roots")] = roots;
    root[QStringLiteral("activeByCategory")] = activeByCategory;
    root[QStringLiteral("nodes")] = nodes;

    QSaveFile out(m_filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(logCore) << "[ContextGraph] Failed to write context graph:" << m_filePath << out.errorString();
        return;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!out.commit())
        qCWarning(logCore) << "[ContextGraph] Failed to commit context graph:" << m_filePath << out.errorString();
}

void ContextGraph::clear()
{
    m_nodes.clear();
    m_categoryRootNodeIds.clear();
    m_activeNodeByCategory.clear();
    m_lastActiveNodeId.clear();
    m_nodeCounter = 0;
    ensureCategoryRootNode(QStringLiteral("general"));
    save();
}
