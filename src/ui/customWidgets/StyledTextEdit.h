#pragma once

#include <QTextEdit>

class StyledTextEdit : public QTextEdit
{
    Q_OBJECT
public:
    explicit StyledTextEdit(QWidget *parent = nullptr)
        : QTextEdit(parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QTextEdit {"
            "  border: 2px solid #5A9;"            // светло-синий контур
            "  border-radius: 12px;"               // округлые углы
            "  padding: 6px 12px;"                 // внутренние отступы
            "  font: 18px \"Segoe UI\", sans-serif;" // крупный шрифт
            "  background-color: #ffffff;"         // белый фон
            "  color: #000000;"                    // черный текст
            "}"
        );
    }
};