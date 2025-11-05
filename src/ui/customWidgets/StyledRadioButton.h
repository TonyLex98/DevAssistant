#pragma once

#include <QRadioButton>

class StyledRadioButton : public QRadioButton
{
    Q_OBJECT
public:
    explicit StyledRadioButton(const QString &text = {}, QWidget *parent = nullptr)
        : QRadioButton(text, parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QRadioButton {"
            "  font: 18px \"Arial\", sans-serif;" // крупный шрифт
            "  spacing: 8px;"                        // расстояние между радио-кнопкой и текстом
            "  color: #000000;"                      // черный текст
            "}"
            "QRadioButton::indicator {"
            "  width: 18px;"
            "  height: 18px;"
            "  border: 2px solid #5A9;"              // светло-синий контур
            "  border-radius: 9px;"                  // круглая форма
            "  background-color: #ffffff;"           // белый фон
            "}"
            "QRadioButton::indicator:checked {"
            "  background-color: #5A9;"              // синий фон при выборе
            "  border: 2px solid #5A9;"
            "}"
            "QRadioButton::indicator:unchecked {"
            "  background-color: #ffffff;"           // белый фон при отмене выбора
            "}"
        );
    }
};
