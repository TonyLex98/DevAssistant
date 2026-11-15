#pragma once
#include <QDialog>
#include <InputTextEdit.h>
#include <StyledButton.h>
class CodeEditor;
class CppHighlighter;


// -----------------------------------------------------
//  MethodEditDialog – code editor window with a message
//  input field and a "Run sequence" button
// -----------------------------------------------------
class MethodEditDialog : public QWidget
{
    Q_OBJECT
public:
    explicit MethodEditDialog(QWidget *parent = nullptr);
    void setCode(const QString &code);
    QString getCode() const;

signals:
    void signal_runSequenceDialog();                      // Run button / Enter in InputTextEdit
    void signal_updatelastUserMessage(const QString& lastUserMessage); // text of the last user message
    void signal_saveMethod();

private:
    CodeEditor      *editor;
    CppHighlighter  *highlighter;
    InputTextEdit   *inputTextEdit;
    StyledButton     *runButton;
    StyledButton *cancelButton;
    StyledButton *saveButton;
};

