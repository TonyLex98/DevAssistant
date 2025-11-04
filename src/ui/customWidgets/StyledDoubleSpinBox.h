#pragma once

#include <QDoubleSpinBox>

class StyledDoubleSpinBox : public QDoubleSpinBox
{
    Q_OBJECT
public:
    explicit StyledDoubleSpinBox(QWidget *parent = nullptr)
        : QDoubleSpinBox(parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QDoubleSpinBox {"
            "  border: 2px solid #5A9;"            // светло-синий контур
            "  border-radius: 12px;"               // округлые углы
            "  padding: 6px 12px;"                 // внутренние отступы
            "  font: 18px \"Segoe UI\", sans-serif;" // крупный шрифт
            "  background-color: #ffffff;"         // белый фон
            "  color: #000000;"                    // черный текст
            "}"
            "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
            "  subcontrol-origin: border;"
            "  subcontrol-position: top right, bottom right;"
            "  width: 20px;"
            "  border-left: 1px solid #5A9;"
            "  border-top-right-radius: 12px;"
            "  border-bottom-right-radius: 12px;"
            "}"
            "QDoubleSpinBox::up-arrow, QDoubleSpinBox::down-arrow {"
            "  width: 14px;"
            "  height: 14px;"
            "}"
        );
    }
};