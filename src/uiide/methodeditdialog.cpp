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

    // 1. Кодовый редактор
    editor = new CodeEditor(this);
    highlighter = new CppHighlighter(editor->document());
    mainLayout->addWidget(editor);

    // 2. Поле ввода + кнопка Run
    QHBoxLayout *inputRow = new QHBoxLayout;
    inputTextEdit = new InputTextEdit(this);
    inputTextEdit->setPlaceholderText("Введите сообщение...");
    inputRow->addWidget(inputTextEdit);

    runButton = new StyledButton("Задать вопрос", this);

    mainLayout->addLayout(inputRow);

    // 3. Строка кнопок «Save / Cancel»
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch(1);

    cancelButton = new StyledButton("Cancel", this);
    saveButton   = new StyledButton("Save",   this);

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
    //  Соединения сигналов
    // -----------------------------------------------------------------
    // 1. Кнопка Run → signal_runSequenceDialog()
    connect(runButton, &StyledButton::clicked,
            this, &MethodEditDialog::signal_runSequenceDialog);

    // 2. Enter в InputTextEdit → signal_runSequenceDialog()
    connect(inputTextEdit, &InputTextEdit::sendMessage,
            this, &MethodEditDialog::signal_runSequenceDialog);

    // 3. При каждом изменении текста в InputTextEdit → signal_updatelastUserMessage()
    connect(inputTextEdit, &InputTextEdit::signal_textEdited,
            this, &MethodEditDialog::signal_updatelastUserMessage);




    // Настройки окна
    setWindowTitle("Edit Block");
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
