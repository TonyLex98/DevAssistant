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

    // красные = путь существует; зелёные = родитель для отсутствующего пути
    void setMarkSets(const QSet<QString>& red, const QSet<QString>& green,
                     int greenAlpha = 90, int redAlpha = 70) {
        m_markRed   = red;
        m_markGreen = green;
        m_greenAlpha = greenAlpha;
        m_redAlpha   = redAlpha;
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, idx);

        // сначала рисуем как обычно
        QStyledItemDelegate::paint(p, o, idx);

        // красим ровно ОДИН раз — из колонки 0, но на всю строку
        if (idx.column() != 0) return;

        const QString path = m_fsModel->filePath(idx);

        QColor overlay;
        if (m_markRed.contains(path)) {
            overlay = QColor(255, 0, 0, m_redAlpha);      // существует → красный фон
        } else if (m_markGreen.contains(path)) {
            overlay = QColor(0, 255, 0, m_greenAlpha);    // отсутствует → зелёный фон
        } else {
            return;
        }

        // прямоугольник на всю ширину строки
        QRect fullRowRect = o.rect;
        if (o.widget) {
            fullRowRect.setLeft(0);
            fullRowRect.setRight(o.widget->width());
        }

        p->save();
        p->setPen(Qt::NoPen);
        p->fillRect(fullRowRect, overlay); // поверх текста/иконок — полупрозрачный слой
        p->restore();
    }

private:
    QFileSystemModel* m_fsModel = nullptr;
    QSet<QString> m_markRed;
    QSet<QString> m_markGreen;
    int m_greenAlpha = 90;
    int m_redAlpha   = 70;
};
