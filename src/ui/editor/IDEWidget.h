#pragma once
// Upgraded IDEWidget with CodeEditor (highlighting + folding) and AST views.
#include <QTreeView>
#include <QFileSystemModel>
#include <QSplitter>
#include <QJsonObject>
#include <QMap>
#include <tree_sitter/api.h>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QSyntaxHighlighter>
#include "ui/editor/cpphighlighter.h"
#include "ui/editor/PythonHighlighter.h"
#include "ui/editor/DartHighlighter.h"
#include "ui/editor/methodeditdialog.h"
#include "ui/widgets/MemorySplitter.h"
#include <QPainter>
#include "ui/editor/PathColorDelegate.h"
#include "ui/widgets/StyledTabWidget.h"
#include "ui/widgets/StyledTreeWidget.h"

class CodeEditor;
class ASTGraphView;


class IDEWidget : public MemorySplitter {
    Q_OBJECT
public:
    IDEWidget(const QString &key,
              Qt::Orientation orientation = Qt::Horizontal,
              QWidget *parent = nullptr);
    ~IDEWidget();

    void openProjectbyPath(const QString &path);
    // Shows the file in the editor and selects it in the tree.
    void openFile(const QString& filePath);


public slots:
    void updateMissingFiles(const QSet<QString> &paths);

private slots:
    void onFileClicked(const QModelIndex &index);
    void onTreeItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onGraphOpenAt(int line, int column);

    // Handles Ctrl+S
    void handleSaveRequest();

    void onGraphNodeDoubleClicked(const QJsonObject &node);

    void slot_saveMethod();



signals:
    void fileModified(bool modified);
    void signal_runSequenceDialog();                      // Run button / Enter in InputTextEdit
    void signal_updatelastUserMessage(const QString& lastUserMessage); // text of the last user message

private:
    QTreeView *dirView;
    QFileSystemModel *dirModel;
    CodeEditor *codeEditor;
    StyledTreeWidget *astTree;
    ASTGraphView *astGraph;
    StyledTabWidget *rightTabs;

    QString currentFilePath;
    std::string sourceCode;
    QJsonObject astJson;
    QMap<QTreeWidgetItem*, QJsonObject*> itemToJsonMap;

    void parseFile(const QString &filePath);
    void buildAstTree(const QJsonObject &node, QTreeWidgetItem *parentItem);
    QString reconstructCode(const QJsonObject &obj);
    QJsonObject nodeToJson(const TSNode& node, const std::string& source);
    void updateFoldsFromAst();

    QSyntaxHighlighter* createHighlighter(const QString &ext);

    QString getCurrentFilePath() const { return currentFilePath; }

    // IDEWidget.cpp



    // Language map
    QMap<QString, const TSLanguage *(*)()> langMap;


    QSyntaxHighlighter *currentHighlighter = nullptr;   // active syntax highlighter

    MethodEditDialog *methodEdit;

    QTreeWidgetItem *m_current_tree_item = nullptr;

    PathColorDelegate* m_pathDelegate = nullptr;
    QSet<QString> m_highlightPaths;
};
