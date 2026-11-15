#pragma once

#include <QTreeWidget>

class StyledTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit StyledTreeWidget(QWidget *parent = nullptr)
        : QTreeWidget(parent)
    {
        setStyleSheet(
            "QTreeWidget {"
            "  border: 1px solid #8ed9cf;"
            "  border-radius: 6px;"
            "  font: 10pt \"Segoe UI\";"
            "  color: #0f172a;"
            "  background-color: #ffffff;"
            "  padding: 2px;"
            "}"
            "QTreeWidget::item {"
            "  padding: 4px 8px;"
            "  border-bottom: 1px solid #e6f7f3;"
            "}"
            "QTreeWidget::item:selected {"
            "  background-color: #ccfbf1;"
            "  color: #0f172a;"
            "}"
            "QTreeWidget::item:hover {"
            "  background-color: #e6fffa;"
            "}"
            "QHeaderView::section {"
            "  background: #e6fffa;"
            "  border: none;"
            "  border-bottom: 1px solid #bfeee6;"
            "  padding: 4px 8px;"
            "  font: 600 9pt \"Segoe UI\";"
            "}"
        );
    }
};
