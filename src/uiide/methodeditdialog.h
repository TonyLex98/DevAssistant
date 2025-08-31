#pragma once
#include <QDialog>
#include <InputTextEdit.h>
#include <StyledButton.h>
class CodeEditor;
class CppHighlighter;


// -----------------------------------------------------
//  MethodEditDialog – диалог редактирования кода с
//  кнопкой “Run sequence” и полем ввода сообщения
// -----------------------------------------------------
class MethodEditDialog : public QWidget
{
    Q_OBJECT
public:
    explicit MethodEditDialog(QWidget *parent = nullptr);
    void setCode(const QString &code);
    QString getCode() const;

signals:
    void signal_runSequenceDialog();                      // Кнопка Run / Enter в InputTextEdit
    void signal_updatelastUserMessage(const QString& lastUserMessage); // Текст последнего сообщения
    void signal_saveMethod();

private:
    CodeEditor      *editor;
    CppHighlighter  *highlighter;
    InputTextEdit   *inputTextEdit;
    StyledButton     *runButton;
    StyledButton *cancelButton;
    StyledButton *saveButton;
};

