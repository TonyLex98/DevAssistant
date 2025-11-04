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
            "  font: 18px \"Segoe UI\", sans-serif;" // крупный шрифт
            "  color: #000000;"                      // черный текст
            "  padding: 6px 12px;"                   // внутренние отступы
            "}"
        );
    }
    
    explicit StyledLabel(const QString &text, QWidget *parent = nullptr)
        : QLabel(text, parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QLabel {"
            "  font: 18px \"Segoe UI\", sans-serif;" // крупный шрифт
            "  color: #000000;"                      // черный текст
            "  padding: 6px 12px;"                   // внутренние отступы
            "}"
        );
    }
};