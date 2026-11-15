// methodeditdialog.cpp
#include "methodeditdialog.h"
#include "codeeditor.h"
#include "cpphighlighter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <StyledButton.h>
#include <QKeyEvent>

MethodEditDialog::MethodEditDialog(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 1. Code editor
    editor = new CodeEditor(this);
    highlighter = new CppHighlighter(editor->document());
    mainLayout->addWidget(editor);

    // 2. Message input + Run button
    QHBoxLayout *inputRow = new QHBoxLayout;
    inputTextEdit = new InputTextEdit(this);
    inputTextEdit->setPlaceholderText(tr("Type a message..."));
    inputRow->addWidget(inputTextEdit);

    runButton = new StyledButton(tr("Ask"), this);

    mainLayout->addLayout(inputRow);

    // 3. Save / Cancel buttons
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch(1);

    cancelButton = new StyledButton(tr("Cancel"), this);
    saveButton   = new StyledButton(tr("Save"),   this);

    connect(cancelButton, &StyledButton::clicked,
            this, &QWidget::hide);
    connect(saveButton,   &StyledButton::clicked,
            this, [this](){
        emit signal_saveMethod();
    });

    btnRow->addWidget(runButton);
    btnRow->addWidget(cancelButton);
    btnRow->addWidget(saveButton);

    mainLayout->addLayout(btnRow);

    // -----------------------------------------------------------------
    //  Signal connections
    // -----------------------------------------------------------------
    // 1. Run button → signal_runSequenceDialog()
    connect(runButton, &StyledButton::clicked,
            this, &MethodEditDialog::signal_runSequenceDialog);

    // 2. Enter in InputTextEdit → signal_runSequenceDialog()
    connect(inputTextEdit, &InputTextEdit::sendMessage,
            this, &MethodEditDialog::signal_runSequenceDialog);

    // 3. Every text change in InputTextEdit → signal_updatelastUserMessage()
    connect(inputTextEdit, &InputTextEdit::signal_textEdited,
            this, &MethodEditDialog::signal_updatelastUserMessage);




    setWindowTitle(tr("Edit Block"));
    resize(800, 500);
}

void MethodEditDialog::setCode(const QString &code)
{
    editor->setPlainText(code);
}

QString MethodEditDialog::getCode() const
{
    return editor->toPlainText();
}
