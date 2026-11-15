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
            "  font: 18px \"Arial\", sans-serif;"
            "  spacing: 8px;"
            "  color: #000000;"
            "}"
            "QCheckBox::indicator {"
            "  width: 18px;"
            "  height: 18px;"
            "  border: 2px solid #5A9;"
            "  border-radius: 4px;"
            "  background-color: #ffffff;"
            "}"
            "QCheckBox::indicator:checked {"
            "  background-color: #5A9;"
            "  image: url(:/checked.png);"
            "}"
            "QCheckBox::indicator:unchecked {"
            "  background-color: #ffffff;"
            "}"
        );
    }
};
