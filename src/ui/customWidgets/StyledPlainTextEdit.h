#pragma once

#include <QPlainTextEdit>

class StyledPlainTextEdit : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit StyledPlainTextEdit(QWidget *parent = nullptr)
        : QPlainTextEdit(parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QPlainTextEdit {"
            "  border: 2px solid #5A9;"              // светло-синий контур
            "  border-radius: 12px;"                 // округлые углы
            "  padding: 6px 12px;"                   // внутренние отступы
            "  font: 18px \"Arial\", sans-serif;" // крупный шрифт
            "  background-color: #ffffff;"           // белый фон
            "  color: #000000;"                      // черный текст
            "}"
        );
    }
};
