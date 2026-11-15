#pragma once

#include <QTextEdit>

class StyledTextEdit : public QTextEdit
{
    Q_OBJECT
public:
    explicit StyledTextEdit(QWidget *parent = nullptr)
        : QTextEdit(parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QTextEdit {"
            "  border: 2px solid #5A9;"
            "  border-radius: 12px;"
            "  padding: 6px 12px;"
            "  font: 18px \"Arial\", sans-serif;"
            "  background-color: #ffffff;"
            "  color: #000000;"
            "}"
        );

        // Remove any size constraints to allow unlimited text
        setMaximumHeight(QWIDGETSIZE_MAX);  // Remove height limit
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);  // Ensure scroll bar appears when needed
    }
};
