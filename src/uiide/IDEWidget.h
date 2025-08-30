#pragma once
// Upgraded IDEWidget with CodeEditor (highlighting + folding) and AST views.
#include <QTreeView>
#include <QFileSystemModel>
#include <QTreeWidget>
#include <QSplitter>
#include <QJsonObject>
#include <QMap>
#include <QTabWidget>
#include <tree_sitter/api.h>
#include <QPushButton>           // <‑‑ добавить
#include <QVBoxLayout>
#include <QFileDialog>
#include <QSyntaxHighlighter>
#include <cpphighlighter.h>
#include <PythonHighlighter.h>
#include <DartHighlighter.h>

class CodeEditor;
class ASTGraphView;

class IDEWidget : public QSplitter {
    Q_OBJECT
public:
    IDEWidget(QWidget *parent = nullptr);
    ~IDEWidget();

    void openProjectbyPath(const QString &path);

private slots:
    void onFileClicked(const QModelIndex &index);
    void onTreeItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onGraphOpenAt(int line, int column);

    // Внутренний слот, обрабатывающий Ctrl+S
    void handleSaveRequest();


signals:
    void fileModified(bool modified);

private:
    QTreeView *dirView;
    QFileSystemModel *dirModel;
    CodeEditor *codeEditor;
    QTreeWidget *astTree;
    ASTGraphView *astGraph;
    QTabWidget *rightTabs;

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

    //QPushButton *openProjectBtn;    // ← новая кнопка

    QSyntaxHighlighter *currentHighlighter = nullptr;   // ← хранит активный подсветчик
};
