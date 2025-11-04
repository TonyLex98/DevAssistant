#pragma once

#include <QTabBar>
#include <QStyleOptionTab>
#include <QPainter>
#include <QStyle>

/**
 * @brief CustomTextTabBar - кастомная вкладка, которая подстраивает ширину под текст
 *
 * Этот класс переопределяет поведение QTabBar для корректного подгонки ширины вкладок под текст.
 * В отличие от стандартного поведения setExpanding, этот класс вычисляет размер каждой вкладки
 * на основе длины текста и внутренних отступов.
 */
class CustomTextTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit CustomTextTabBar(QWidget *parent = nullptr)
        : QTabBar(parent)
    {
        setExpanding(true);  // Отключаем равномерное расширение
        //setElideMode(Qt::ElideNone);  // Не обрезаем текст
        setDrawBase(false);  // Убираем базовую линию, если не нужна
        //setStyleSheet("QTabBar::tab { margin: 1; padding: 1; }");

    }

protected:
    /**
     * @brief Переопределенный метод для вычисления размера вкладки
     * @param index - индекс вкладки
     * @return QSize - оптимальный размер для вкладки с указанным текстом
     */
    QSize tabSizeHint(int index) const override
    {
        if (index >= 0 && index < count()) {
            QString text = tabText(index);
            QFontMetrics fm = fontMetrics();

            // Вычисляем ширину текста
            int textWidth = fm.width(text);

            // Определяем размеры с учетом стиля
            QStyleOptionTab opt;
            initStyleOption(&opt, index);

            // Вычисляем размер, необходимый для отображения текста
            // с учетом внутренних отступов и стилевых параметров
            QSize textSize(textWidth, fm.height());
            QSize size = style()->sizeFromContents(QStyle::CT_TabBarTab, &opt, textSize, this);

            // Увеличиваем ширину, чтобы обеспечить комфортное отображение текста
            int padding = 30; // Добавляем дополнительный отступ
            size.setWidth(size.width() + padding);

            // Устанавливаем минимальную ширину для читаемости
            int minWidth = 80;
            size.setWidth(qMax(minWidth, size.width()));

            // Устанавливаем максимальную ширину, чтобы табы не были слишком широкими
            int maxWidth = 400;
            size.setWidth(qMin(maxWidth, size.width()));

            return size;
        }


        return QTabBar::tabSizeHint(index);
    }

    /**
     * @brief Переопределенный метод для вычисления минимального размера вкладки
     */
    QSize minimumTabSizeHint(int index) const override
    {
        if (index >= 0 && index < count()) {
            QString text = tabText(index);
            QFontMetrics fm = fontMetrics();

            int textWidth = fm.width(text);
            int minWidth = qMax(60, textWidth + 10); // Минимум 60 или текст + небольшие отступы

            // Ограничиваем минимальную ширину
            minWidth = qMin(minWidth, 150);

            return QSize(minWidth, fm.height() + 10); // Высота текста + отступы
        }

        return QTabBar::minimumTabSizeHint(index);
    }
};
