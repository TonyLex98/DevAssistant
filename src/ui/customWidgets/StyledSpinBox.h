#pragma once

#include <QSpinBox>

class StyledSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    explicit StyledSpinBox(QWidget *parent = nullptr)
        : QSpinBox(parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QSpinBox {"
            "  border: 2px solid #5A9;"
            "  border-radius: 12px;"
            "  padding: 6px 12px;"
            "  font: 18px \"Arial\", sans-serif;"
            "  background-color: #ffffff;"
            "  color: #000000;"
            "}"
            "QSpinBox::up-button, QSpinBox::down-button {"
            "  subcontrol-origin: border;"
            "  subcontrol-position: top right, bottom right;"
            "  width: 20px;"
            "  border-left: 1px solid #5A9;"
            "  border-top-right-radius: 12px;"
            "  border-bottom-right-radius: 12px;"
            "}"
            "QSpinBox::up-arrow, QSpinBox::down-arrow {"
            "  width: 14px;"
            "  height: 14px;"
            "}"
        );
    }
};
