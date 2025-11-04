#pragma once

#include <QTabWidget>

class StyledTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    explicit StyledTabWidget(QWidget *parent = nullptr)
        : QTabWidget(parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QTabWidget::pane {"
            "  border: 2px solid #5A9;"              // светло-синий контур
            "  border-radius: 12px;"                 // округлые углы
            "  background-color: #f0f4ff;"            // светлый фон
            "}"
            "QTabBar::tab {"
            "  font: 18px \"Segoe UI\", sans-serif;" // крупный шрифт
            "  color: #000000;"                      // черный текст
            "  background-color: #e8f0ff;"           // светло-голубой фон
            "  border: 2px solid #5A9;"              // светло-синий контур
            "  border-top-left-radius: 10px;"        // округлые углы сверху
            "  border-top-right-radius: 10px;"       // округлые углы сверху
            "  padding: 8px 16px;"                   // внутренние отступы
            "  margin-right: 2px;"                   // отступ справа между табами
            "}"
            "QTabBar::tab:selected {"
            "  background-color: #ffffff;"           // белый фон у выбранного таба
            "  border-bottom-color: #f0f4ff;"        // цвет нижней границы совпадает с панелью
            "}"
            "QTabBar::tab:hover {"
            "  background-color: #c8def8;"           // подсветка при наведении
            "}"
        );
    }
};