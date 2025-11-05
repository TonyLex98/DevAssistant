#pragma once

#include <QProgressBar>

class StyledProgressBar : public QProgressBar
{
    Q_OBJECT
public:
    explicit StyledProgressBar(QWidget *parent = nullptr)
        : QProgressBar(parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QProgressBar {"
            "  border: 2px solid #5A9;"              // светло-синий контур
            "  border-radius: 12px;"                 // округлые углы
            "  text-align: center;"                  // выравнивание текста по центру
            "  font: 18px \"Arial\", sans-serif;" // крупный шрифт
            "  color: #000000;"                      // черный текст
            "  background-color: #ffffff;"           // белый фон
            "}"
            "QProgressBar::chunk {"
            "  background-color: #5A9;"              // синий цвет заполнения
            "  border-radius: 10px;"                 // округлые углы у заполнения
            "}"
        );
    }
};
