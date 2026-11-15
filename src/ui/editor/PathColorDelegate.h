// PathColorDelegate.h
#pragma once
#include <QStyledItemDelegate>
#include <QFileSystemModel>
#include <QSet>
#include <QPainter>
#include <QFileInfo>
#include <QDir>

class PathColorDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit PathColorDelegate(QFileSystemModel* fsm, QObject* parent=nullptr)
        : QStyledItemDelegate(parent), m_fsModel(fsm) {}

    // red = path exists; green = parent of a missing path
    void setMarkSets(const QSet<QString>& red, const QSet<QString>& green,
                     int greenAlpha = 28, int redAlpha = 28) {
        m_markRed   = red;
        m_markGreen = green;
        m_greenAlpha = greenAlpha;
        m_redAlpha   = redAlpha;
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, idx);

        // default painting first
        QStyledItemDelegate::paint(p, o, idx);

        // paint the overlay once, from column 0, across the whole row
        if (idx.column() != 0) return;

        const QString path = m_fsModel->filePath(idx);

        QColor overlay;
        if (m_markRed.contains(path)) {
            overlay = QColor(245, 158, 11, m_redAlpha);   // exists, no description yet: amber
        } else if (m_markGreen.contains(path)) {
            overlay = QColor(16, 185, 129, m_greenAlpha); // missing: green
        } else {
            return;
        }

        // full row width
        QRect fullRowRect = o.rect;
        if (o.widget) {
            fullRowRect.setLeft(0);
            fullRowRect.setRight(o.widget->width());
        }

        p->save();
        p->setPen(Qt::NoPen);
        p->fillRect(fullRowRect, overlay); // translucent layer over text and icons
        p->restore();
    }

private:
    QFileSystemModel* m_fsModel = nullptr;
    QSet<QString> m_markRed;
    QSet<QString> m_markGreen;
    int m_greenAlpha = 28;
    int m_redAlpha   = 28;
};
