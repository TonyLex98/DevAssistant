#pragma once
#include <QObject>
#include <QStringList>
#include <QDirIterator>
#include "SymbolModel.h"
#include <tree_sitter/api.h>

extern "C" const TSLanguage *tree_sitter_cpp();
extern "C" const TSLanguage *tree_sitter_python();   // <-- добавлено
extern "C" const TSLanguage *tree_sitter_dart();     // <-- добавлено




class ProjectIndexer : public QObject {
    Q_OBJECT
public:
    explicit ProjectIndexer(QObject *parent=nullptr);

    struct Settings { QStringList includeExt{ ".h", ".hpp", ".hh", ".cpp", ".cc", ".cxx" }; };
    void setRoot(const QString &rootDir){ m_root = rootDir; }
    void setSettings(const Settings &s){ m_settings=s; }

    // sync build for validation/diff
    SymbolGraph buildGraphSync();

signals:
    void graphReady(const SymbolGraph &graph);
    void progress(const QString &msg);

public slots:
    void rebuild();

private:
    QString m_root; Settings m_settings;

    // helpers
    QString computeSid(NodeKind kind, const QString &canonical, const QString &file, const QString &signature) const;
    void indexFileTS(const QString &filePath, SymbolGraph &g, const TSLanguage *lang);
    void collectEdgesCalls(TSNode node, const QByteArray &src, const QString &file, const NodeData &currentMethod, SymbolGraph &g);

    // small utils
    static QString tsNodeText(TSNode n, const QByteArray &src);
    static int tsNodeLine(TSNode n, const QByteArray &src);

    // Tree-sitter members
    struct TSState;
    TSState* m_state;
};
