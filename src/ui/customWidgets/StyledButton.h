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
        // ──  Стиль, одинаковый с MemoryComboBox/MemoryLineEdit ──
        setStyleSheet(
            "QPushButton {"
            "  border: 2px solid #5A9;"            // светло‑синий контур
            "  border-radius: 12px;"              // округлые углы
            "  padding: 6px 12px;"                // внутренние отступы
            "  font: 18px \"Arial\", sans-serif;" // крупный шрифт
            "  background-color: #e8f0ff;"        // мягкий голубой фон
            "}"
            "QPushButton:hover { background-color: #c8def8; }"  // подсветка при наведении
            "QPushButton:pressed { background-color: #a8c0f8; }" // при клике
        );
    }
};
