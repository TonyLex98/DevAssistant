#include "methodeditdialog.h"
#include "codeeditor.h"
#include "cpphighlighter.h"
#include <QVBoxLayout>
#include <QPushButton>

MethodEditDialog::MethodEditDialog(QWidget *parent) : QDialog(parent) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    editor = new CodeEditor(this);
    highlighter = new CppHighlighter(editor->document());
    layout->addWidget(editor);

    auto *row = new QWidget(this);
    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0,0,0,0);
    QPushButton *saveButton = new QPushButton("Save", this);
    QPushButton *cancelButton = new QPushButton("Cancel", this);
    connect(saveButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    rowLay->addStretch();
    rowLay->addWidget(cancelButton);
    rowLay->addWidget(saveButton);
    layout->addWidget(row);

    setWindowTitle("Edit Block");
    resize(800, 500);
}

void MethodEditDialog::setCode(const QString &code) {
    editor->setPlainText(code);
}

QString MethodEditDialog::getCode() const {
    return editor->toPlainText();
}
