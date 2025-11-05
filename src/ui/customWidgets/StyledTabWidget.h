#pragma once

#include <QTabWidget>
#include "CustomTextTabBar.h"  // Include our custom tab bar

/**
 * @brief StyledTabWidget - Custom QTabWidget with tabs that expand to fit text content
 * 
 * This widget extends QTabWidget to provide:
 * - Tabs that expand to accommodate their text content
 * - Consistent styling with rounded corners and gradient backgrounds
 * - Proper padding and spacing for better visual appearance
 */
class StyledTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    explicit StyledTabWidget(QWidget *parent = nullptr)
        : QTabWidget(parent)
    {

        // Replace the default tab bar with our custom tab bar that sizes tabs based on text
        CustomTextTabBar *customTabBar = new CustomTextTabBar(this);
        setTabBar(customTabBar);
        
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QTabWidget::pane {"
            "  border: 2px solid #5A9;"              // светло-синий контур
            "  border-radius: 12px;"                 // округлые углы
            "  background-color: #f0f4ff;"            // светлый фон
            "}"
            "QTabBar::tab {"
            "  font: 18px \"Arial\", sans-serif;" // крупный шрифт
            "  color: #000000;"                      // черный текст
            "  background-color: #ffffff;"           // светло-голубой фон
            "  border: 2px solid #5A9;"              // светло-синий контур
            "  border-top-left-radius: 10px;"        // округлые углы сверху
            "  border-top-right-radius: 10px;"       // округлые углы сверху
            "  padding: 10px 10px;"                   // внутренние отступы
            "  margin-right: 2px;"                   // отступ справа между табами
            "}"
            "QTabBar::tab:selected {"
            "  background-color: #e8f0ff;"           // белый фон у выбранного таба
            "  border-bottom-color: #f0f4ff;"        // цвет нижней границы совпадает с панелью
            "}"
            "QTabBar::tab:hover {"
            "  background-color: #c8def8;"           // подсветка при наведении
            "}"
        );
    }

private:
    void updateTabBarGeometry()
    {
        // Принудительно пересчитываем размеры вкладок после добавления новой
        tabBar()->updateGeometry();
        updateGeometry(); // Обновляем геометрию всего виджета
    }
};
