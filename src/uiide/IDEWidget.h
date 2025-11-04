#pragma once
// Upgraded IDEWidget with CodeEditor (highlighting + folding) and AST views.
#include <QTreeView>
#include <QFileSystemModel>
#include <QSplitter>
#include <QJsonObject>
#include <QMap>
#include <tree_sitter/api.h>
#include <QPushButton>           // <‑‑ добавить
#include <QVBoxLayout>
#include <QFileDialog>
#include <QSyntaxHighlighter>
#include <cpphighlighter.h>
#include <PythonHighlighter.h>
#include <DartHighlighter.h>
#include <methodeditdialog.h>
#include <MemorySplitter.h>
#include <QPainter>
#include <PathColorDelegate.h>
#include "../ui/customWidgets/StyledTabWidget.h"
#include "../ui/customWidgets/StyledTreeWidget.h"

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


public slots:
    void updateMissingFiles(const QSet<QString> &paths);

private slots:
    void onFileClicked(const QModelIndex &index);
    void onTreeItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onGraphOpenAt(int line, int column);

    // Внутренний слот, обрабатывающий Ctrl+S
    void handleSaveRequest();

    void onGraphNodeDoubleClicked(const QJsonObject &node);

    void slot_saveMethod();



signals:
    void fileModified(bool modified);
    void signal_runSequenceDialog();                      // Кнопка Run / Enter в InputTextEdit
    void signal_updatelastUserMessage(const QString& lastUserMessage); // Текст последнего сообщения

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

    // IDEWidget.cpp (или отдельный cpp, но в этом примере в IDEWidget.cpp)

    QSyntaxHighlighter* createHighlighter(const QString &ext);

    QString getCurrentFilePath() const { return currentFilePath; }   // <-- геттер

    // IDEWidget.cpp



    // Language map
    QMap<QString, const TSLanguage *(*)()> langMap;


    QSyntaxHighlighter *currentHighlighter = nullptr;   // ← хранит активный подсветчик

    MethodEditDialog *methodEdit;

    QTreeWidgetItem *m_current_tree_item = nullptr;

    PathColorDelegate* m_pathDelegate = nullptr;
    QSet<QString> m_highlightPaths;
};
