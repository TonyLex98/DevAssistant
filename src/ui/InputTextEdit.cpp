#include "InputTextEdit.h"

InputTextEdit::InputTextEdit(QWidget *parent) : StyleTextEdit(parent)
{

    connect(this, &InputTextEdit::textChanged, this, [this](){
        emit signal_textEdited(toPlainText());
    });
}
