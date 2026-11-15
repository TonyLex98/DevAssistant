#pragma once

#include <QLabel>

class StyledLabel : public QLabel
{
    Q_OBJECT
public:
    explicit StyledLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QLabel {"
            "  font: 18px \"Arial\", sans-serif;"
            "  color: #000000;"
            "  padding: 6px 12px;"
            "}"
        );
    }
    
    explicit StyledLabel(const QString &text, QWidget *parent = nullptr)
        : QLabel(text, parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QLabel {"
            "  font: 18px \"Arial\", sans-serif;"
            "  color: #000000;"
            "  padding: 6px 12px;"
            "}"
        );
    }
};
