#include "ProjectIndexer.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>

#include <functional>

namespace {

enum class Family { Cpp, Python, Dart };

// Node types differ between grammars; these are the ones the indexer looks for.
struct LanguageSpec {
    Family family;
    QSet<QString> classTypes;     // class definitions
    QSet<QString> functionTypes;  // function and method definitions
    QString bodyType;             // the function body node, where calls are looked for
    QString callType;             // call expression; empty = calls are not indexed
};

const LanguageSpec &specFor(const TSLanguage *lang)
{
    static const LanguageSpec cpp{Family::Cpp,
            {QStringLiteral("class_specifier"), QStringLiteral("struct_specifier")},
            {QStringLiteral("function_definition"), QStringLiteral("function_declaration")},
            QStringLiteral("compound_statement"), QStringLiteral("call_expression")};
    static const LanguageSpec python{Family::Python,
            {QStringLiteral("class_definition")},
            {QStringLiteral("function_definition")},
            QStringLiteral("block"), QStringLiteral("call")};
    static const LanguageSpec dart{Family::Dart,
            {QStringLiteral("class_definition")},
            {QStringLiteral("function_signature")},
            QString(), QString()};
    if (lang == tree_sitter_python())
        return python;
    if (lang == tree_sitter_dart())
        return dart;
    return cpp;
}

QByteArray readFile(const QString &filePath)
{
    QFile f(filePath);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QString nodeType(TSNode n)
{
    return QString::fromUtf8(ts_node_type(n));
}

// The last part of a callee expression: "ns::f" -> "f", "obj->run" -> "run", "self.x" -> "x".
QString lastSegment(QString name)
{
    name.remove(QRegularExpression(QStringLiteral("\\s+")));
    const int paren = name.indexOf(QLatin1Char('<'));
    if (paren > 0)
        name = name.left(paren);
    for (const QString &sep : {QStringLiteral("::"), QStringLiteral("->"), QStringLiteral(".")}) {
        const int i = name.lastIndexOf(sep);
        if (i >= 0)
            name = name.mid(i + sep.size());
    }
    return name;
}

bool isSkippedDir(const QString &path)
{
    static const QStringList skipped = {
        QStringLiteral("/.git/"), QStringLiteral("/node_modules/"), QStringLiteral("/DevelopersAssistant/"),
        QStringLiteral("/build/"), QStringLiteral("/.dart_tool/"), QStringLiteral("/__pycache__/"),
        QStringLiteral("/.venv/"), QStringLiteral("/venv/"),
    };
    const QString p = QDir::fromNativeSeparators(path);
    for (const QString &s : skipped) {
        if (p.contains(s))
            return true;
    }
    return false;
}

QString ensureSyntheticClass(SymbolGraph &g, const QString &name, const QString &fileForHash)
{
    const QString sid = QString::fromUtf8(QCryptographicHash::hash(
            (QString::number(int(NodeKind::Class)) + "|" + name + "|" + fileForHash + "|").toUtf8(),
            QCryptographicHash::Sha1).toHex());
    if (!g.nodes.contains(sid)) {
        NodeData cls;
        cls.kind = NodeKind::Class;
        cls.displayName = name;
        cls.canonicalName = name;
        cls.loc.file = fileForHash;
        cls.loc.line = 1;
        cls.sid.id = sid;
        g.nodes.insert(sid, cls);
    }
    return sid;
}

} // namespace

struct ProjectIndexer::TSState {
    TSParser *parser = nullptr;
};

ProjectIndexer::ProjectIndexer(QObject *parent)
    : QObject(parent), m_state(new TSState)
{
    m_state->parser = ts_parser_new();
}

ProjectIndexer::~ProjectIndexer()
{
    ts_parser_delete(m_state->parser);
    delete m_state;
}

const TSLanguage *ProjectIndexer::languageForSuffix(const QString &suffixWithDot)
{
    const QString s = suffixWithDot.toLower();
    if (s == ".cpp" || s == ".cc" || s == ".cxx" || s == ".c" || s == ".h" || s == ".hh" || s == ".hpp")
        return tree_sitter_cpp();
    if (s == ".py")
        return tree_sitter_python();
    if (s == ".dart")
        return tree_sitter_dart();
    return nullptr;
}

void ProjectIndexer::rebuild()
{
    emit progress(tr("Indexing the project with tree-sitter..."));
    emit graphReady(buildGraphSync());
}

QString ProjectIndexer::computeSid(NodeKind kind, const QString &canonical, const QString &file, const QString &signature) const
{
    const QByteArray base = (QString::number(int(kind)) + "|" + canonical + "|" + file + "|" + signature).toUtf8();
    return QString::fromUtf8(QCryptographicHash::hash(base, QCryptographicHash::Sha1).toHex());
}

SymbolGraph ProjectIndexer::buildGraphSync()
{
    SymbolGraph g;
    m_pendingCalls.clear();
    if (m_root.isEmpty())
        return g;

    QDirIterator it(m_root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (isSkippedDir(path))
            continue;
        if (!languageForSuffix("." + QFileInfo(path).suffix()))
            continue;
        indexFile(path, g);
    }
    resolveCalls(g);
    return g;
}

QString ProjectIndexer::tsNodeText(TSNode n, const QByteArray &src)
{
    if (ts_node_is_null(n))
        return QString();
    const uint32_t start = ts_node_start_byte(n);
    const uint32_t end = ts_node_end_byte(n);
    if (start > uint32_t(src.size()) || end > uint32_t(src.size()) || end < start)
        return QString();
    return QString::fromUtf8(src.mid(int(start), int(end - start)));
}

int ProjectIndexer::tsNodeLine(TSNode n)
{
    return int(ts_node_start_point(n).row) + 1;
}

void ProjectIndexer::collectCalls(TSNode body, const QByteArray &src, const QString &callType, const NodeData &caller)
{
    if (callType.isEmpty())
        return;
    int order = 0;
    std::function<void(TSNode)> walk = [&](TSNode n) {
        if (nodeType(n) == callType) {
            const QString name = lastSegment(tsNodeText(ts_node_child(n, 0), src));
            if (!name.isEmpty())
                m_pendingCalls.push_back({caller.sid.id, name, ++order});
        }
        const uint32_t count = ts_node_child_count(n);
        for (uint32_t i = 0; i < count; ++i)
            walk(ts_node_child(n, i));
    };
    walk(body);
}

void ProjectIndexer::indexFile(const QString &filePath, SymbolGraph &g)
{
    const TSLanguage *lang = languageForSuffix("." + QFileInfo(filePath).suffix());
    if (!lang)
        return;
    const QByteArray src = readFile(filePath);
    if (src.isEmpty())
        return;

    // The file itself.
    NodeData fileNode;
    fileNode.kind = NodeKind::Language;
    fileNode.displayName = QFileInfo(filePath).fileName();
    fileNode.canonicalName = fileNode.displayName;
    fileNode.loc.file = filePath;
    fileNode.sid.id = computeSid(NodeKind::Language, fileNode.canonicalName, filePath, "");
    g.nodes.insert(fileNode.sid.id, fileNode);

    ts_parser_set_language(m_state->parser, lang);
    TSTree *tree = ts_parser_parse_string(m_state->parser, nullptr, src.constData(), uint32_t(src.size()));
    if (!tree)
        return;
    const LanguageSpec &spec = specFor(lang);
    const TSNode root = ts_tree_root_node(tree);

    const auto childText = [&](TSNode n, const QSet<QString> &types) {
        const uint32_t count = ts_node_child_count(n);
        for (uint32_t i = 0; i < count; ++i) {
            const TSNode ch = ts_node_child(n, i);
            if (types.contains(nodeType(ch)))
                return tsNodeText(ch, src).trimmed();
        }
        return QString();
    };
    static const QSet<QString> nameTypes = {QStringLiteral("type_identifier"), QStringLiteral("identifier")};

    // Pass 1: classes, so that out-of-class definitions (A::f) find their class.
    QHash<QString, QString> classSidByName;
    std::function<void(TSNode)> findClasses = [&](TSNode n) {
        if (spec.classTypes.contains(nodeType(n))) {
            const QString name = childText(n, nameTypes);
            if (!name.isEmpty()) {
                const QString sid = computeSid(NodeKind::Class, name, filePath, "");
                if (!g.nodes.contains(sid)) {
                    NodeData cls;
                    cls.kind = NodeKind::Class;
                    cls.displayName = name;
                    cls.canonicalName = name;
                    cls.loc.file = filePath;
                    cls.loc.line = tsNodeLine(n);
                    cls.sid.id = sid;
                    g.nodes.insert(sid, cls);
                }
                classSidByName.insert(name, sid);
            }
        }
        const uint32_t count = ts_node_child_count(n);
        for (uint32_t i = 0; i < count; ++i)
            findClasses(ts_node_child(n, i));
    };
    findClasses(root);

    const QString globalContainer = ensureSyntheticClass(
            g, QStringLiteral("__Global::%1").arg(QFileInfo(filePath).fileName()), filePath);

    // Pass 2: functions, methods and fields, with the class they belong to.
    std::function<void(TSNode, const QString &)> visit = [&](TSNode n, const QString &scopeClassSid) {
        const QString type = nodeType(n);

        if (spec.classTypes.contains(type)) {
            const QString name = childText(n, nameTypes);
            const QString scope = name.isEmpty() ? scopeClassSid : classSidByName.value(name, scopeClassSid);
            const uint32_t count = ts_node_child_count(n);
            for (uint32_t i = 0; i < count; ++i)
                visit(ts_node_child(n, i), scope);
            return;
        }

        // C++ data members: `int x;` is a field_declaration with a field_identifier child.
        if (spec.family == Family::Cpp && type == QLatin1String("field_declaration") && !scopeClassSid.isEmpty()) {
            const QString field = childText(n, {QStringLiteral("field_identifier")});
            if (!field.isEmpty()) {
                NodeData fld;
                fld.kind = NodeKind::Field;
                fld.displayName = field;
                fld.canonicalName = field;
                fld.loc.file = filePath;
                fld.loc.line = tsNodeLine(n);
                fld.parentSid = scopeClassSid;
                fld.sid.id = computeSid(fld.kind, field, filePath, "");
                g.nodes.insert(fld.sid.id, fld);
            }
        }

        if (spec.functionTypes.contains(type)) {
            QString name, params, retType;
            TSNode body{};
            bool hasBody = false;
            const uint32_t count = ts_node_child_count(n);
            for (uint32_t i = 0; i < count; ++i) {
                const TSNode ch = ts_node_child(n, i);
                const QString t = nodeType(ch);
                if (spec.family == Family::Cpp) {
                    if (t == QLatin1String("type_identifier") || t == QLatin1String("primitive_type"))
                        retType = tsNodeText(ch, src).trimmed();
                    if (t == QLatin1String("function_declarator")) {
                        const uint32_t dc = ts_node_child_count(ch);
                        for (uint32_t j = 0; j < dc; ++j) {
                            const TSNode d = ts_node_child(ch, j);
                            const QString dt = nodeType(d);
                            if (dt == QLatin1String("qualified_identifier") || dt == QLatin1String("identifier")
                                    || dt == QLatin1String("field_identifier"))
                                name = tsNodeText(d, src).trimmed();
                            else if (dt == QLatin1String("parameter_list"))
                                params = tsNodeText(d, src).trimmed();
                        }
                    }
                } else {
                    if (t == QLatin1String("identifier") && name.isEmpty())
                        name = tsNodeText(ch, src).trimmed();
                    else if (t == QLatin1String("parameters") || t == QLatin1String("formal_parameter_list"))
                        params = tsNodeText(ch, src).trimmed();
                }
                if (!spec.bodyType.isEmpty() && t == spec.bodyType) {
                    body = ch;
                    hasBody = true;
                }
            }

            if (!name.isEmpty()) {
                // The owning class: the enclosing class, or A in A::f.
                QString parentSid = scopeClassSid;
                if (name.contains(QLatin1String("::"))) {
                    const QStringList parts = name.split(QStringLiteral("::"));
                    const QString className = parts.at(parts.size() - 2);
                    parentSid = classSidByName.contains(className)
                            ? classSidByName.value(className)
                            : ensureSyntheticClass(g, className, filePath);
                }
                if (parentSid.isEmpty())
                    parentSid = globalContainer;

                NodeData fn;
                fn.kind = NodeKind::Method;
                fn.displayName = name + params;
                fn.canonicalName = name;
                fn.signature = retType + params;
                fn.loc.file = filePath;
                fn.loc.line = tsNodeLine(n);
                fn.parentSid = parentSid;
                fn.sid.id = computeSid(fn.kind, fn.canonicalName, filePath, fn.signature);
                g.nodes.insert(fn.sid.id, fn);

                if (hasBody)
                    collectCalls(body, src, spec.callType, fn);
            }
        }

        const uint32_t count = ts_node_child_count(n);
        for (uint32_t i = 0; i < count; ++i)
            visit(ts_node_child(n, i), scopeClassSid);
    };
    visit(root, QString());

    ts_tree_delete(tree);
}

void ProjectIndexer::resolveCalls(SymbolGraph &g)
{
    // Every definition by its simple name ("A::f" -> "f"). With several candidates the call
    // goes to the first one found; overloads and same-named methods are not told apart.
    QHash<QString, QString> definitionByName;
    for (auto it = g.nodes.constBegin(); it != g.nodes.constEnd(); ++it) {
        if (it->kind == NodeKind::Method || it->kind == NodeKind::Function) {
            const QString simple = lastSegment(it->canonicalName);
            if (!definitionByName.contains(simple))
                definitionByName.insert(simple, it.key());
        }
    }

    for (const PendingCall &call : m_pendingCalls) {
        QString toSid = definitionByName.value(call.calleeName);
        if (toSid.isEmpty()) {
            toSid = computeSid(NodeKind::Function, call.calleeName, QString(), QString());
            if (!g.nodes.contains(toSid)) {
                NodeData ext;
                ext.kind = NodeKind::Function;
                ext.displayName = call.calleeName + "()";
                ext.canonicalName = call.calleeName;
                ext.sid.id = toSid;
                ext.parentSid = ensureSyntheticClass(g, QStringLiteral("__External"), QString());
                g.nodes.insert(toSid, ext);
            }
        }
        EdgeData e;
        e.fromSid = call.fromSid;
        e.toSid = toSid;
        e.kind = EdgeKind::Calls;
        e.order = call.order;
        e.label = QString::number(call.order);
        g.edges.push_back(e);
    }
    m_pendingCalls.clear();
}
