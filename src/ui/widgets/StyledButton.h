// StyledButton.h
#pragma once

#include <QPushButton>

class StyledButton : public QPushButton
{
    Q_OBJECT
public:
    explicit StyledButton(const QString &text = {}, QWidget *parent = nullptr)
        : QPushButton(text, parent)
    {
        setStyleSheet(
            "QPushButton {"
            "  border: 1px solid #0f766e;"
            "  border-radius: 8px;"
            "  padding: 5px 10px;"
            "  min-height: 30px;"
            "  font: 10pt \"Segoe UI\";"
            "  font-weight: 600;"
            "  background-color: #14b8a6;"
            "  color: #083b36;"
            "}"
            "QPushButton:hover { background-color: #2dd4bf; }"
            "QPushButton:pressed { background-color: #0f766e; color: #ecfeff; }"
            "QPushButton:disabled {"
            "  background-color: #99f6e4;"
            "  color: #0f766e;"
            "  border-color: #5eead4;"
            "}"
        );
    }
};
