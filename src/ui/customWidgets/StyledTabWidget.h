#pragma once

#include <QTabWidget>
#include "CustomTextTabBar.h"

class StyledTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    explicit StyledTabWidget(QWidget *parent = nullptr)
        : QTabWidget(parent)
    {
        auto *customTabBar = new CustomTextTabBar(this);
        setTabBar(customTabBar);

        setStyleSheet(
            "QTabWidget::pane {"
            "  border: 1px solid #8ed9cf;"
            "  border-radius: 6px;"
            "  background-color: #ffffff;"
            "}"
            "QTabBar::tab {"
            "  font: 600 10pt \"Segoe UI Variable\";"
            "  color: #0f172a;"
            "  background-color: #dff8f3;"
            "  border: 1px solid #8ed9cf;"
            "  border-bottom: none;"
            "  border-top-left-radius: 6px;"
            "  border-top-right-radius: 6px;"
            "  padding: 5px 10px;"
            "  margin-right: 1px;"
            "}"
            "QTabBar::tab:selected {"
            "  background-color: #ffffff;"
            "  color: #0f766e;"
            "}"
            "QTabBar::tab:hover {"
            "  background-color: #cff3eb;"
            "}"
        );
    }
};
