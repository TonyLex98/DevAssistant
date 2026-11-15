#pragma once

#include <QTabBar>
#include <QStyleOptionTab>
#include <QPainter>
#include <QStyle>

/**
 * @brief Tab bar whose tab widths follow the tab text.
 *
 * Unlike the default setExpanding behaviour, each tab is sized from its
 * text width plus the style's internal padding.
 */
class CustomTextTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit CustomTextTabBar(QWidget *parent = nullptr)
        : QTabBar(parent)
    {
        setExpanding(true);
        setElideMode(Qt::ElideNone);  // never elide tab text
        setDrawBase(false);  // no base line under the tabs
        //setStyleSheet("QTabBar::tab { margin: 1; padding: 1; }");

    }

protected:
    /**
     * @brief Size of a tab, fitted to its text.
     * @param index tab index
     * @return preferred size for the tab
     */
    QSize tabSizeHint(int index) const override
    {
        if (index >= 0 && index < count()) {
            QString text = tabText(index);
            QFontMetrics fm = fontMetrics();

            int textWidth = fm.width(text);

            QStyleOptionTab opt;
            initStyleOption(&opt, index);

            // Size needed for the text including the style's padding
            QSize textSize(textWidth, fm.height());
            QSize size = style()->sizeFromContents(QStyle::CT_TabBarTab, &opt, textSize, this);

            // Extra room so the text does not look cramped
            int padding = 40;
            size.setWidth(size.width() + padding);

            // Minimum width for readability
            int minWidth = 80;
            size.setWidth(qMax(minWidth, size.width()));

            // Cap the width so tabs do not get too wide
            int maxWidth = 400;
            size.setWidth(qMin(maxWidth, size.width()));

            return size;
        }


        return QTabBar::tabSizeHint(index);
    }

    /**
     * @brief Minimum size of a tab.
     */
    QSize minimumTabSizeHint(int index) const override
    {
        if (index >= 0 && index < count()) {
            QString text = tabText(index);
            QFontMetrics fm = fontMetrics();

            int textWidth = fm.width(text);

            QStyleOptionTab opt;
            initStyleOption(&opt, index);

            // Size needed for the text including the style's padding
            QSize textSize(textWidth, fm.height());
            QSize size = style()->sizeFromContents(QStyle::CT_TabBarTab, &opt, textSize, this);

            // Smaller padding for the minimum size
            int padding = 20;
            size.setWidth(size.width() + padding);

            // Minimum width for readability
            int minWidth = 60;
            size.setWidth(qMax(minWidth, size.width()));

            // Upper bound for the minimum width
            int maxWidth = 150;
            size.setWidth(qMin(maxWidth, size.width()));

            return size;
        }

        return QTabBar::minimumTabSizeHint(index);
    }
};
