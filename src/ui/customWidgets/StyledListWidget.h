#pragma once

#include <QListWidget>

class StyledListWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit StyledListWidget(QWidget *parent = nullptr)
        : QListWidget(parent)
    {
        // Consistent styling with other custom widgets
        setStyleSheet(
            "QListWidget {"
            "  border: 2px solid #5A9;"              // светло-синий контур
            "  border-radius: 12px;"                 // округлые углы
            "  font: 18px \"Segoe UI\", sans-serif;" // крупный шрифт
            "  color: #000000;"                      // черный текст
            "  background-color: #ffffff;"           // белый фон
            "  padding: 4px;"                        // внутренние отступы
            "}"
            "QListWidget::item {"
            "  padding: 6px 12px;"                   // отступы для элементов
            "  border-bottom: 1px solid #e0e0e0;"    // разделитель между элементами
            "}"
            "QListWidget::item:selected {"
            "  background-color: #c8def8;"           // подсветка выбранного элемента
            "  color: #000000;"                      // черный текст
            "}"
            "QListWidget::item:hover {"
            "  background-color: #d8e4f8;"           // подсветка при наведении
            "}"
        );
    }
};