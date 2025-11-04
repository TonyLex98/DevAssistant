#pragma once

#include <QCheckBox>

class StyledCheckBox : public QCheckBox
{
    Q_OBJECT
public:
    explicit StyledCheckBox(const QString &text = {}, QWidget *parent = nullptr)
        : QCheckBox(text, parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QCheckBox {"
            "  font: 18px \"Segoe UI\", sans-serif;" // крупный шрифт
            "  spacing: 8px;"                        // расстояние между чекбоксом и текстом
            "  color: #000000;"                      // черный текст
            "}"
            "QCheckBox::indicator {"
            "  width: 18px;"
            "  height: 18px;"
            "  border: 2px solid #5A9;"              // светло-синий контур
            "  border-radius: 4px;"                  // слегка закругленные углы
            "  background-color: #ffffff;"           // белый фон
            "}"
            "QCheckBox::indicator:checked {"
            "  background-color: #5A9;"              // синий фон при выборе
            "  image: url(:/checked.png);"
            "}"
            "QCheckBox::indicator:unchecked {"
            "  background-color: #ffffff;"           // белый фон при отмене выбора
            "}"
        );
    }
};