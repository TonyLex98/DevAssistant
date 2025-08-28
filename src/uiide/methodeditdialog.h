#pragma once
#include <QDialog>
class CodeEditor;
class CppHighlighter;

class MethodEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit MethodEditDialog(QWidget *parent = nullptr);
    void setCode(const QString &code);
    QString getCode() const;
private:
    CodeEditor *editor;
    CppHighlighter *highlighter;
};
