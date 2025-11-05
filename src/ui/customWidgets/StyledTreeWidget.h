#pragma once

#include <QTreeWidget>

class StyledTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit StyledTreeWidget(QWidget *parent = nullptr)
        : QTreeWidget(parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QTreeWidget {"
            "  border: 2px solid #5A9;"              // светло-синий контур
            "  border-radius: 12px;"                 // округлые углы
            "  font: 18px \"Arial\", sans-serif;" // крупный шрифт
            "  color: #000000;"                      // черный текст
            "  background-color: #ffffff;"           // белый фон
            "  padding: 4px;"                        // внутренние отступы
            "}"
            "QTreeWidget::item {"
            "  padding: 6px 12px;"                   // отступы для элементов
            "  border-bottom: 1px solid #e0e0e0;"    // разделитель между элементами
            "}"
            "QTreeWidget::item:selected {"
            "  background-color: #c8def8;"           // подсветка выбранного элемента
            "  color: #000000;"                      // черный текст
            "}"
            "QTreeWidget::item:hover {"
            "  background-color: #d8e4f8;"           // подсветка при наведении
            "}"
            "QTreeWidget::branch {"
            "  background-color: #ffffff;"
            "}"
            "QTreeWidget::branch:selected {"
            "  background-color: #c8def8;"
            "}"
            "QTreeWidget::branch:has-children:!has-siblings:closed,"
            "QTreeWidget::branch:has-children:open {"
            "  border-image: none;"
            "  image: none;"
            "}"
        );
    }
};
