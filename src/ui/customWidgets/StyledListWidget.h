#pragma once

#include <QListWidget>

class StyledListWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit StyledListWidget(QWidget *parent = nullptr)
        : QListWidget(parent)
    {
        setStyleSheet(
            "QListWidget {"
            "  border: 1px solid #8ed9cf;"
            "  border-radius: 6px;"
            "  font: 10pt \"Segoe UI Variable\";"
            "  color: #0f172a;"
            "  background-color: #ffffff;"
            "  padding: 2px;"
            "}"
            "QListWidget::item {"
            "  padding: 4px 8px;"
            "  border-bottom: 1px solid #e6f7f3;"
            "}"
            "QListWidget::item:selected {"
            "  background-color: #ccfbf1;"
            "  color: #0f172a;"
            "}"
            "QListWidget::item:hover {"
            "  background-color: #e6fffa;"
            "}"
        );
    }
};
