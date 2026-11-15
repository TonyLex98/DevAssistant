#pragma once
#include <QDirIterator>
#include <QObject>
#include <QStringList>
#include <QVector>
#include "core/code/SymbolModel.h"
#include <tree_sitter/api.h>

extern "C" const TSLanguage *tree_sitter_cpp();
extern "C" const TSLanguage *tree_sitter_python();
extern "C" const TSLanguage *tree_sitter_dart();

// Builds a symbol graph of a source tree with tree-sitter: files, classes, functions and
// methods, data members, and call edges between functions.
//
// Each file is parsed with its own grammar (C/C++, Python or Dart, by extension). Calls are
// resolved after every file is indexed, so a call in one file links to the definition in
// another; calls to functions defined nowhere in the project go to an "__External" group.
// Dart files contribute classes and functions but no call edges.
class ProjectIndexer : public QObject {
    Q_OBJECT
public:
    explicit ProjectIndexer(QObject *parent=nullptr);
    ~ProjectIndexer() override;

    struct Settings { QStringList includeExt{ ".h", ".hpp", ".hh", ".cpp", ".cc", ".cxx" }; };
    void setRoot(const QString &rootDir){ m_root = rootDir; }
    void setSettings(const Settings &s){ m_settings=s; }

    // Indexes the whole tree under the root and returns the graph.
    SymbolGraph buildGraphSync();

    // Indexes one file into `g` (used by buildGraphSync and by tests). Call edges found in it
    // are kept pending until resolveCalls().
    void indexFile(const QString &filePath, SymbolGraph &g);
    // Turns the pending calls into edges, linking each one to a definition when one exists.
    void resolveCalls(SymbolGraph &g);

    // The grammar for a file extension such as ".py", or nullptr if it is not indexed.
    static const TSLanguage *languageForSuffix(const QString &suffixWithDot);

signals:
    void graphReady(const SymbolGraph &graph);
    void progress(const QString &msg);

public slots:
    void rebuild();

private:
    struct PendingCall { QString fromSid; QString calleeName; int order = 0; };

    QString m_root; Settings m_settings;
    QVector<PendingCall> m_pendingCalls;

    QString computeSid(NodeKind kind, const QString &canonical, const QString &file, const QString &signature) const;
    void collectCalls(TSNode body, const QByteArray &src, const QString &callType, const NodeData &caller);

    static QString tsNodeText(TSNode n, const QByteArray &src);
    static int tsNodeLine(TSNode n);

    struct TSState;
    TSState* m_state;
};
