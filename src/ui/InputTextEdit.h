#pragma once
#include <QDialog>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QPushButton>
#include "../adapters/OllamaAdapter.h"
#include <QKeyEvent>
#include <StyleTextEdit.h>

class InputTextEdit : public StyleTextEdit {
    Q_OBJECT
public:
    explicit InputTextEdit(QWidget* parent = nullptr);
signals:
    void sendMessage();
    void signal_textEdited(const QString & text);
protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Return && !(e->modifiers() & Qt::ShiftModifier)) {
            emit sendMessage();
        } else {
            QTextEdit::keyPressEvent(e);
        }
    }
};

