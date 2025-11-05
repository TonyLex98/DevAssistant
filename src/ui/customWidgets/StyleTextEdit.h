// StyleTextEdit.h
#pragma once

#include <QTextEdit>
#include <QKeyEvent>
#include <QDebug>

/**
 * @brief StyleTextEdit
 *        QTextEdit, стилизованный как MemoryComboBox.
 *
 * В конструкторе задаётся:
 *   - 2‑пиксельный серо‑синий бордер (#5A9)
 *   - 12‑пиксельное скругление углов
 *   - padding: 6px 12px
 *   - font: 18px "Arial", sans-serif
 *
 * При необходимости можно переопределить keyPressEvent
 * (например, чтобы посылать сообщение по Enter).
 */
class StyleTextEdit : public QTextEdit
{
    Q_OBJECT
public:
    explicit StyleTextEdit(QWidget *parent = nullptr)
        : QTextEdit(parent)
    {
        // --------- Стили, как у MemoryComboBox ----------
        setStyleSheet(
            "QTextEdit {"
            "  border: 2px solid #5A9;"        // 2px бордер, цвет #5A9
            "  border-radius: 12px;"          // скругление 12px
            "  padding: 6px 12px;"            // внутренние отступы
            "  font: 18px \"Arial\", sans-serif;"  // шрифт
            "}"
        );
        qDebug() << "[StyleTextEdit] ctor, parent =" << parent;
    }

protected:

};
