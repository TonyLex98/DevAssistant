#include "ProjectIndexer.h"
#include <QFile>
#include <QTextStream>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDebug>

// ProjectIndexer.cpp
QMap<QString, const TSLanguage*(*)()> langMap = {
    {".cpp",  tree_sitter_cpp},
    {".c",    tree_sitter_cpp},
    {".h",    tree_sitter_cpp},
    {".hpp",  tree_sitter_cpp},
    {".py",   tree_sitter_python},
    {".dart", tree_sitter_dart}
};

// Внутренняя структура с парсером
struct ProjectIndexer::TSState{
    TSParser* parser = nullptr;
};

// --- вот тут уже нормальный КОНСТРУКТОР, а не глобальный код ---
ProjectIndexer::ProjectIndexer(QObject *parent): QObject(parent){
    m_state = new TSState();
    m_state->parser = ts_parser_new();
    ts_parser_set_language(m_state->parser, tree_sitter_cpp());
}


void ProjectIndexer::rebuild(){
    emit progress("Сканирую проект (Tree-sitter, global-containers)...");
    SymbolGraph g = buildGraphSync();
    emit graphReady(g);
}

QString ProjectIndexer::computeSid(NodeKind kind, const QString &canonical, const QString &file, const QString &signature) const{
    QByteArray base = (QString::number((int)kind) + "|" + canonical + "|" + file + "|" + signature).toUtf8();
    auto h = QCryptographicHash::hash(base, QCryptographicHash::Sha1).toHex();
    return QString::fromUtf8(h);
}

SymbolGraph ProjectIndexer::buildGraphSync() {
    SymbolGraph g;
    if (m_root.isEmpty()) return g;

    QDirIterator it(m_root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()){
        const QString path = it.next();
        const QString ext = QFileInfo(path).suffix().toLower();
        const QString extWithDot = "." + ext;
        if (!langMap.contains(extWithDot)) continue;          // только нужные языки

        // Добавляем узел «файл» уровня Language
        NodeData fileNode;
        fileNode.kind = NodeKind::Language;                    // новый тип
        fileNode.displayName = QFileInfo(path).fileName();
        fileNode.canonicalName = fileNode.displayName;
        fileNode.loc.file = path;
        fileNode.loc.line = 1;
        fileNode.loc.column = 1;
        fileNode.sid.id = computeSid(NodeKind::Language, fileNode.canonicalName, path, "");
        g.nodes.insert(fileNode.sid.id, fileNode);

        // Парсим содержимое с помощью Tree‑sitter
        indexFileTS(path, g, langMap[extWithDot]());          // <‑‑ новый параметр
    }
    return g;
}


static QByteArray readAllUtf8(const QString &filePath){
    QFile f(filePath);
    if(!f.open(QIODevice::ReadOnly)) return QByteArray();
    QByteArray b = f.readAll();
    f.close();
    return b;
}

QString ProjectIndexer::tsNodeText(TSNode n, const QByteArray &src){
    if(ts_node_is_null(n)) return QString();
    uint32_t start = ts_node_start_byte(n);
    uint32_t end = ts_node_end_byte(n);
    if(start>src.size() || end>src.size() || end<start) return QString();
    return QString::fromUtf8(src.mid(start, end-start));
}

int ProjectIndexer::tsNodeLine(TSNode n, const QByteArray &src){
    Q_UNUSED(src);
    TSPoint s = ts_node_start_point(n);
    return (int)s.row + 1;
}

static QString simpleTrimIdent(QString s){
    s.remove(QRegularExpression("\\s+"));
    return s;
}

static QString ensureSyntheticClass(SymbolGraph &g, const QString &name, const QString &fileForHash){
    QString canonical = name;
    QString sid = QString::fromUtf8(QCryptographicHash::hash(
        (QString::number((int)NodeKind::Class) + "|" + canonical + "|" + fileForHash + "|").toUtf8(),
        QCryptographicHash::Sha1).toHex());
    if(!g.nodes.contains(sid)){
        NodeData cls; cls.kind=NodeKind::Class; cls.displayName=name; cls.canonicalName=canonical;
        cls.signature=""; cls.loc.file=fileForHash; cls.loc.line=1;
        cls.sid.id = sid; g.nodes.insert(sid, cls);
    }
    return sid;
}

void ProjectIndexer::collectEdgesCalls(TSNode node, const QByteArray &src, const QString &file, const NodeData &currentMethod, SymbolGraph &g){
    std::function<void(TSNode,int&)> walk;
    walk = [&](TSNode n, int &order){
        QString t = QString::fromUtf8(ts_node_type(n));
        if(t=="call_expression"){
            TSNode funcNode = ts_node_child(n, 0);
            QString fname = tsNodeText(funcNode, src).trimmed();
            fname = simpleTrimIdent(fname);
            if(!fname.isEmpty()){
                QString calleeSid = computeSid(NodeKind::Function, fname, "", "");
                if(!g.nodes.contains(calleeSid)){
                    // Put unknown callee into synthetic External container
                    QString extClass = ensureSyntheticClass(g, "__External", "");
                    NodeData cnode; cnode.kind=NodeKind::Function; cnode.displayName=fname+"()"; cnode.canonicalName=fname;
                    cnode.sid.id=calleeSid; cnode.loc.file=""; cnode.parentSid = extClass;
                    g.nodes.insert(cnode.sid.id, cnode);
                } else {
                    // if exists but has no parent, attach to External
                    NodeData &cn = g.nodes[calleeSid];
                    if(cn.parentSid.isEmpty()){
                        cn.parentSid = ensureSyntheticClass(g, "__External", "");
                    }
                }
                EdgeData e; e.fromSid=currentMethod.sid.id; e.toSid=calleeSid; e.kind=EdgeKind::Calls; e.order=++order; e.label=QString::number(e.order);
                g.edges.push_back(e);
            }
        }
        uint32_t cnt = ts_node_child_count(n);
        for(uint32_t i=0;i<cnt;++i) walk(ts_node_child(n,i), order);
    };
    int order=0; walk(node, order);
}

// ProjectIndexer.cpp
void ProjectIndexer::indexFileTS(const QString &filePath, SymbolGraph &g, const TSLanguage *lang) {
    QByteArray src = readAllUtf8(filePath);
    if (src.isEmpty()) return;

    TSTree *tree = ts_parser_parse_string(m_state->parser, nullptr, src.constData(), (uint32_t)src.size());
    if (!tree) return;

    TSNode root = ts_tree_root_node(tree);

    // Collect classes map name->sid
    QHash<QString, QString> classSidByName;
    std::function<void(TSNode)> pass1 = [&](TSNode n){
        QString type = QString::fromUtf8(ts_node_type(n));
        if(type=="class_specifier" || type=="struct_specifier"){
            QString className;
            uint32_t cc = ts_node_child_count(n);
            for(uint32_t i=0;i<cc;++i){
                TSNode ch = ts_node_child(n,i);
                QString t = QString::fromUtf8(ts_node_type(ch));
                if(t=="type_identifier" || t=="identifier"){
                    className = tsNodeText(ch, src).trimmed(); break;
                }
            }
            if(!className.isEmpty()){
                QString sid = computeSid(NodeKind::Class, className, filePath, "");
                if(!g.nodes.contains(sid)){
                    NodeData cls; cls.kind=NodeKind::Class; cls.displayName=className; cls.canonicalName=className;
                    cls.signature=""; cls.loc.file=filePath; cls.loc.line=tsNodeLine(n, src);
                    cls.sid.id = sid; g.nodes.insert(sid, cls);
                }
                classSidByName.insert(className, sid);
            }
        }
        uint32_t cnt = ts_node_child_count(n);
        for(uint32_t i=0;i<cnt;++i) pass1(ts_node_child(n,i));
    };
    pass1(root);

    // synthetic per-file global container
    QString globalContainer = ensureSyntheticClass(g, QString("__Global::%1").arg(QFileInfo(filePath).fileName()), filePath);

    std::function<void(TSNode, QString /*scopeClassSid*/)> visit;
    visit = [&](TSNode n, QString scopeClassSid){
        const QString type = QString::fromUtf8(ts_node_type(n));

        if(type=="class_specifier" || type=="struct_specifier"){
            QString className;
            uint32_t cc = ts_node_child_count(n);
            for(uint32_t i=0;i<cc;++i){
                TSNode ch = ts_node_child(n,i);
                QString t = QString::fromUtf8(ts_node_type(ch));
                if(t=="type_identifier" || t=="identifier"){
                    className = tsNodeText(ch, src).trimmed(); break;
                }
            }
            QString newScope = scopeClassSid;
            if(className.size()){
                newScope = classSidByName.value(className);
            }
            uint32_t cnt = ts_node_child_count(n);
            for(uint32_t i=0;i<cnt;++i) visit(ts_node_child(n,i), newScope);
            return;
        }

        if(type=="field_declaration"){
            // only attach if in class scope
            if(!scopeClassSid.isEmpty()){
                QString fieldName;
                uint32_t cc = ts_node_child_count(n);
                for(uint32_t i=0;i<cc;++i){
                    TSNode ch = ts_node_child(n,i);
                    QString t = QString::fromUtf8(ts_node_type(ch));
                    if(t=="field_declarator" || t=="identifier"){
                        fieldName = tsNodeText(ch, src).trimmed();
                        break;
                    }
                }
                if(!fieldName.isEmpty()){
                    NodeData fld; fld.kind=NodeKind::Field; fld.displayName=fieldName; fld.canonicalName=fieldName;
                    fld.signature=""; fld.loc.file=filePath; fld.loc.line=tsNodeLine(n, src);
                    fld.parentSid = scopeClassSid; fld.sid.id = computeSid(fld.kind, fld.canonicalName, filePath, fld.signature);
                    g.nodes.insert(fld.sid.id, fld);
                }
            }
        }

        if(type=="function_declaration" || type=="function_definition"){
            QString retType, funcName, paramsSig;
            uint32_t cc = ts_node_child_count(n);
            for(uint32_t i=0;i<cc;++i){
                TSNode ch = ts_node_child(n,i);
                QString t = QString::fromUtf8(ts_node_type(ch));
                if(t=="type_identifier" || t=="primitive_type") retType = tsNodeText(ch, src).trimmed();
                if(t=="function_declarator"){
                    uint32_t dc = ts_node_child_count(ch);
                    for(uint32_t j=0;j<dc;++j){
                        TSNode d = ts_node_child(ch,j);
                        QString dt = QString::fromUtf8(ts_node_type(d));
                        if(dt=="qualified_identifier" || dt=="identifier"){
                            funcName = tsNodeText(d, src).trimmed();
                        } else if(dt=="parameter_list"){
                            paramsSig = tsNodeText(d, src).trimmed();
                        }
                    }
                }
            }
            if(!funcName.isEmpty()){
                // Prefer class scope: by current scope or by qualified name A::B::foo
                QString parentSid = scopeClassSid;
                if(funcName.contains("::")){
                    QStringList parts = funcName.split("::");
                    if(parts.size()>=2){
                        QString className = parts.at(parts.size()-2);
                        if(classSidByName.contains(className)){
                            parentSid = classSidByName.value(className);
                        } else {
                            // create placeholder class for fq name
                            parentSid = ensureSyntheticClass(g, className, filePath);
                        }
                    }
                }
                if(parentSid.isEmpty()){
                    // force into per-file global container
                    parentSid = globalContainer;
                }
                NodeData fn; fn.kind=NodeKind::Method; fn.displayName=funcName+paramsSig; fn.canonicalName=funcName;
                fn.signature=retType+paramsSig; fn.loc.file=filePath; fn.loc.line=tsNodeLine(n, src);
                fn.parentSid = parentSid;
                fn.sid.id = computeSid(fn.kind, fn.canonicalName, filePath, fn.signature);
                g.nodes.insert(fn.sid.id, fn);

                // collect calls for definitions
                if(type=="function_definition"){
                    uint32_t cc2 = ts_node_child_count(n);
                    for(uint32_t k=0;k<cc2;++k){
                        TSNode ch2 = ts_node_child(n,k);
                        if(QString::fromUtf8(ts_node_type(ch2))=="compound_statement"){
                            collectEdgesCalls(ch2, src, filePath, fn, g);
                        }
                    }
                }
            }
        }

        uint32_t cnt = ts_node_child_count(n);
        for(uint32_t i=0;i<cnt;++i) visit(ts_node_child(n,i), scopeClassSid);
    };
    visit(root, QString());

    ts_tree_delete(tree);
}
