#pragma once
#include <QDialog>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QPushButton>
#include "core/model/OllamaAdapter.h"
#include <QKeyEvent>
#include "ui/widgets/StyleTextEdit.h"
#include <QMimeData>
#include <QStringList>

class InputTextEdit : public StyleTextEdit {
    Q_OBJECT
public:
    explicit InputTextEdit(QWidget* parent = nullptr);
signals:
    void sendMessage();
    void signal_textEdited(const QString & text);
    void imagesInserted(const QStringList &filePaths,
                        const QStringList &imagesBase64Png,
                        const QStringList &imageNames);
protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Return && !(e->modifiers() & Qt::ShiftModifier)) {
            emit sendMessage();
        } else {
            QTextEdit::keyPressEvent(e);
        }
    }

    bool canInsertFromMimeData(const QMimeData *source) const override;
    void insertFromMimeData(const QMimeData *source) override;
};
