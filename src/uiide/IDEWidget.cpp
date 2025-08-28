#include "IDEWidget.h"
#include "codeeditor.h"
#include "cpphighlighter.h"
#include "astgraphview.h"
#include "methodeditdialog.h"

#include <QVBoxLayout>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QDir>
#include <QInputDialog>
#include <QTabWidget>
#include <QStatusBar>
#include <QApplication>

extern "C" const TSLanguage *tree_sitter_cpp();

IDEWidget::IDEWidget(QWidget *parent) : QSplitter(parent) {
    // Language map
    langMap[".h"] = tree_sitter_cpp;
    langMap[".cpp"] = tree_sitter_cpp;
    langMap[".hpp"] = tree_sitter_cpp;


    //openProjectBtn = new QPushButton("Открыть проект");
    // Left: directory tree
    dirModel = new QFileSystemModel(this);
    dirModel->setRootPath(QDir::rootPath());
    dirView = new QTreeView(this);
    dirView->setModel(dirModel);
    dirView->setRootIndex(dirModel->index(QDir::homePath()));
    dirView->setHeaderHidden(true);
    connect(dirView, &QTreeView::clicked, this, &IDEWidget::onFileClicked);
    this->addWidget(dirView);

    // Middle: Code editor
    codeEditor = new CodeEditor(this);
    new CppHighlighter(codeEditor->document());
    this->addWidget(codeEditor);

    // Right: tabs with AST (tree + graph)
    rightTabs = new QTabWidget(this);
    astTree = new QTreeWidget(this);
    astTree->setHeaderLabel("AST");
    connect(astTree, &QTreeWidget::itemDoubleClicked, this, &IDEWidget::onTreeItemDoubleClicked);
    rightTabs->addTab(astTree, "Outline");

    astGraph = new ASTGraphView(this);
    connect(astGraph, &ASTGraphView::openAt, this, &IDEWidget::onGraphOpenAt);
    rightTabs->addTab(astGraph, "Graph");

    this->addWidget(rightTabs);
    //addWidget(openProjectBtn);
    //setCentralWidget(this);
    resize(1400, 900);

    //connect(openProjectBtn, &QPushButton::clicked, this, &IDEWidget::onOpenProjectBtnClicked);
    //statusBar()->showMessage("Ready");
}

IDEWidget::~IDEWidget() {}

void IDEWidget::openProjectbyPath(const QString &projectDir)
{
    if (!projectDir.isEmpty()){
        dirView->setRootIndex(dirModel->index(projectDir));
    }
}

void IDEWidget::onFileClicked(const QModelIndex &index) {
    QString filePath = dirModel->filePath(index);
    if (QFileInfo(filePath).isFile()) {
        parseFile(filePath);
    }
}

void IDEWidget::parseFile(const QString &filePath) {
    currentFilePath = filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open file");
        return;
    }
    sourceCode = file.readAll().toStdString();
    file.close();

    QString ext = QFileInfo(filePath).suffix();
    ext = "." + ext;
    if (!langMap.contains(ext)) {
        QMessageBox::warning(this, "Error", "Unsupported file type");
        return;
    }

    const TSLanguage *lang = langMap[ext]();
    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, lang);
    TSTree *tree = ts_parser_parse_string(parser, nullptr, sourceCode.c_str(), (uint32_t)sourceCode.length());
    if (!tree) {
        QMessageBox::warning(this, "Error", "Parsing failed");
        ts_parser_delete(parser);
        return;
    }

    TSNode root = ts_tree_root_node(tree);
    astJson = nodeToJson(root, sourceCode);

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    // Update code editor text
    codeEditor->setPlainText(QString::fromStdString(sourceCode));

    // Build side outline
    astTree->clear();
    itemToJsonMap.clear();
    QTreeWidgetItem *rootItem = new QTreeWidgetItem(astTree, QStringList() << astJson["type"].toString());
    itemToJsonMap[rootItem] = new QJsonObject(astJson);
    buildAstTree(astJson, rootItem);
    astTree->expandToDepth(1);

    // Update graph
    astGraph->setAST(astJson);

    // Compute folds
    updateFoldsFromAst();
}

QJsonObject IDEWidget::nodeToJson(const TSNode& node, const std::string& source) {
    QJsonObject obj;
    obj["type"] = QString(ts_node_type(node));
    obj["start_byte"] = static_cast<qint64>(ts_node_start_byte(node));
    obj["end_byte"] = static_cast<qint64>(ts_node_end_byte(node));
    obj["start_row"] = static_cast<int>(ts_node_start_point(node).row);
    obj["start_column"] = static_cast<int>(ts_node_start_point(node).column);
    obj["end_row"] = static_cast<int>(ts_node_end_point(node).row);
    obj["end_column"] = static_cast<int>(ts_node_end_point(node).column);
    obj["is_named"] = ts_node_is_named(node);
    obj["is_missing"] = ts_node_is_missing(node);
    obj["is_extra"] = ts_node_is_extra(node);

    uint32_t child_count = ts_node_child_count(node);
    if (child_count == 0) {
        size_t start = ts_node_start_byte(node);
        size_t length = ts_node_end_byte(node) - start;
        std::string text = source.substr(start, length);
        obj["text"] = QString::fromStdString(text);
    } else {
        QJsonArray children;
        for (uint32_t i = 0; i < child_count; ++i) {
            TSNode child = ts_node_child(node, i);
            children.append(nodeToJson(child, source));
        }
        obj["children"] = children;
    }

    return obj;
}

void IDEWidget::buildAstTree(const QJsonObject &node, QTreeWidgetItem *parentItem) {
    if (node.contains("children")) {
        QJsonArray children = node["children"].toArray();
        for (const QJsonValue &childVal : children) {
            QJsonObject child = childVal.toObject();
            QString type = child["type"].toString();

            // Show only named nodes at first levels for readability
            if (!child["is_named"].toBool() && parentItem->parent() == nullptr) continue;

            QTreeWidgetItem *childItem = new QTreeWidgetItem(parentItem, QStringList() << type);
            itemToJsonMap[childItem] = new QJsonObject(child);
            buildAstTree(child, childItem);
        }
    } else if (node.contains("text")) {
        parentItem->setText(0, parentItem->text(0) + " : " + node["text"].toString().simplified());
    }
}

/*
QString IDEWidget::reconstructCode(const QJsonObject &obj) {
    if (obj.contains("text")) {
        return obj["text"].toString();
    } else if (obj.contains("children")) {
        QString code;
        QJsonArray children = obj["children"].toArray();
        for (const QJsonValue &childVal : children) {
            code += reconstructCode(childVal.toObject());
        }
        return code;
    }
    return "";
}*/

QString IDEWidget::reconstructCode(const QJsonObject &obj) {
    const int start = obj["start_byte"].toInt();
    const int end   = obj["end_byte"].toInt();
    return QString::fromStdString(sourceCode.substr(start, end - start));
}




void IDEWidget::onTreeItemDoubleClicked(QTreeWidgetItem *item, int) {
    QJsonObject *methodJson = itemToJsonMap.value(item, nullptr);
    if (!methodJson) return;

    const QString type = (*methodJson)["type"].toString();
    if (type.contains("function") || type.contains("method") || type.contains("class") || type.contains("struct")) {
        MethodEditDialog dialog(this);


        size_t start = static_cast<size_t>((*methodJson)["start_byte"].toInt());
        size_t end   = static_cast<size_t>((*methodJson)["end_byte"].toInt());
        QString original = QString::fromStdString(sourceCode.substr(start, end - start));
        dialog.setCode(original);

        //dialog.setCode(reconstructCode(*methodJson));


        if (dialog.exec() == QDialog::Accepted) {
            QString newCode = dialog.getCode();

            size_t start = static_cast<size_t>((*methodJson)["start_byte"].toInt());
            size_t end = static_cast<size_t>((*methodJson)["end_byte"].toInt());
            std::string newSource = sourceCode.substr(0, start) + newCode.toStdString() + sourceCode.substr(end);

            sourceCode = newSource;
            QFile file(currentFilePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(sourceCode.c_str());
                file.close();
            }
            parseFile(currentFilePath);
        }
    }
}

void IDEWidget::onGraphOpenAt(int line, int column) {
    codeEditor->gotoLineColumn(line, column);
    codeEditor->setFocus();
}


void IDEWidget::updateFoldsFromAst() {
    QVector<CodeEditor::FoldRange> ranges;

    std::function<void(const QJsonObject&)> walk = [&](const QJsonObject &node){
        const QString t = node["type"].toString();
        const bool foldable =
                t == "function_definition" ||
                t == "declaration_list" ||
                t == "class_specifier" ||
                t == "struct_specifier" ||
                t == "namespace_definition" ||
                t == "compound_statement";

        if (foldable) {
            int sr = node["start_row"].toInt();
            int er = node["end_row"].toInt();
            if (er > sr) ranges.push_back({sr, er});
        }
        if (node.contains("children")) {
            for (const auto &v : node["children"].toArray()) walk(v.toObject());
        }
    };
    walk(astJson);

    codeEditor->setFoldRanges(ranges);
}
