#pragma once

#include <QGroupBox>

class StyledGroupBox : public QGroupBox
{
    Q_OBJECT
public:
    explicit StyledGroupBox(const QString &title = {}, QWidget *parent = nullptr)
        : QGroupBox(title, parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QGroupBox {"
            "  font: 18px \"Segoe UI\", sans-serif;" // крупный шрифт для заголовка
            "  color: #000000;"                      // черный цвет заголовка
            "  border: 2px solid #5A9;"              // светло-синий контур
            "  border-radius: 12px;"                 // округлые углы
            "  margin-top: 18px;"                    // отступ сверху для заголовка
            "  padding-top: 10px;"                   // отступ внутри сверху
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  left: 10px;"                          // сдвигаем заголовок влево
            "  padding: 0 5px 0 5px;"                // отступы вокруг заголовка
            "}"
        );
    }
};