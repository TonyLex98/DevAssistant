#pragma once
#include <QString>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

// SymbolModel.h
enum class NodeKind { Project, Namespace, Class, Struct, Enum, Method, Field, Function, Language, Unknown };

enum class EdgeKind { Calls, Instantiates, Accesses, Inherits, Uses, ParameterFlow };

struct SourceLoc { QString file; int line = 1; int column = 1; };

struct SymbolId {
    QString id; // stable: kind + canonicalName + filePath + signature hash
    bool operator==(const SymbolId& o) const { return id == o.id; }
};
inline uint qHash(const SymbolId &sid, uint seed = 0){ return qHash(sid.id, seed); }

struct ParamInfo { QString type; QString name; };

struct NodeData {
    SymbolId sid;
    NodeKind kind = NodeKind::Unknown;
    QString displayName;
    QString canonicalName;
    QString signature;
    QString parentSid;   // parent container sid.id (e.g. class)
    SourceLoc loc;
    QVector<ParamInfo> params;
};

struct EdgeData {
    QString fromSid;
    QString toSid;
    EdgeKind kind;
    QString label;
    int order = -1;
};

struct SymbolGraph {
    QHash<QString, NodeData> nodes; // sid.id -> NodeData
    QVector<EdgeData> edges;

    void clear(){ nodes.clear(); edges.clear(); }

    QJsonObject toJson() const {
        QJsonObject obj; QJsonArray jn, je;
        for (const auto &n: nodes){
            QJsonObject o;
            o["sid"] = n.sid.id;
            o["kind"] = (int)n.kind;
            o["displayName"] = n.displayName;
            o["canonicalName"] = n.canonicalName;
            o["signature"] = n.signature;
            o["parentSid"] = n.parentSid;
            o["file"] = n.loc.file;
            o["line"] = n.loc.line;
            o["column"] = n.loc.column;
            QJsonArray jp;
            for (auto &p: n.params){
                QJsonObject po; po["type"]=p.type; po["name"]=p.name; jp.push_back(po);
            }
            o["params"]=jp;
            jn.push_back(o);
        }
        for (const auto &e: edges){
            QJsonObject o;
            o["from"]=e.fromSid; o["to"]=e.toSid; o["kind"]=(int)e.kind; o["label"]=e.label; o["order"]=e.order;
            je.push_back(o);
        }
        obj["nodes"] = jn; obj["edges"] = je; return obj;
    }

    static SymbolGraph fromJson(const QJsonObject &obj){
        SymbolGraph g; auto jn = obj.value("nodes").toArray(); auto je = obj.value("edges").toArray();
        for (auto v: jn){
            auto o=v.toObject(); NodeData n;
            n.sid.id=o.value("sid").toString();
            n.kind=(NodeKind)o.value("kind").toInt();
            n.displayName=o.value("displayName").toString();
            n.canonicalName=o.value("canonicalName").toString();
            n.signature=o.value("signature").toString();
            n.parentSid=o.value("parentSid").toString();
            n.loc.file=o.value("file").toString();
            n.loc.line=o.value("line").toInt(1);
            n.loc.column=o.value("column").toInt(1);
            for (auto pv: o.value("params").toArray()){
                ParamInfo p; auto po=pv.toObject(); p.type=po.value("type").toString(); p.name=po.value("name").toString(); n.params.push_back(p);
            }
            g.nodes.insert(n.sid.id, n);
        }
        for (auto v: je){
            auto o=v.toObject(); EdgeData e;
            e.fromSid=o.value("from").toString(); e.toSid=o.value("to").toString();
            e.kind=(EdgeKind)o.value("kind").toInt(); e.label=o.value("label").toString(); e.order=o.value("order").toInt(-1);
            g.edges.push_back(e);
        }
        return g;
    }
};
