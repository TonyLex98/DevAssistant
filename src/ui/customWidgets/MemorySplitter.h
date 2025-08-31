#pragma once

#include <QSplitter>
#include <QSettings>
#include <QDebug>
#include <QDataStream>   // для сериализации
#include <QByteArray>

class MemorySplitter : public QSplitter
{
    Q_OBJECT
public:
    explicit MemorySplitter(const QString &key,
                            Qt::Orientation orientation = Qt::Horizontal,
                            QWidget *parent = nullptr)
        : QSplitter(orientation, parent), m_key(key)
    {
        qDebug() << "[MemorySplitter] ctor, key =" << m_key;

        /* ---------- Стили ---------- */
        setStyleSheet(
            "QSplitter {"
            "  border: 2px solid #5A9;"
            "  border-radius: 12px;"
            "}"
            "QSplitter::handle {"
            "  background: #5A9;"
            "  width: 8px;"
            "  height: 8px;"
            "}"
        );

        /* Сохраняем при перемещении слайдера */
        connect(this, &QSplitter::splitterMoved,
                this, &MemorySplitter::store);
    }

    /* Позволяет сменить ключ позднее, если понадобится */
    void setKey(const QString &key) { m_key = key; }

    /* Восстановить пропорции вручную */
    void loadState() { load(); }

    ~MemorySplitter() override { store(); }

private:
    /* ---------- Сохраняем в QSettings ---------------- */
    void store()
    {
        QList<int> sz = sizes();          // размеры в пикселях
        qDebug() << "[MemorySplitter] store, sizes =" << sz;

        // сериализуем список в QByteArray
        QByteArray ba;
        QDataStream out(&ba, QIODevice::WriteOnly);
        out << sz;                       // запишем QList<int> в поток

        QSettings s;
        s.setValue(keyPath(), ba);        // сохраняем в настройках
        s.sync();
    }

    /* ---------- Загружаем из QSettings ---------------- */
    void load()
    {
        QSettings s;
        if (!s.contains(keyPath())) {
            qDebug() << "[MemorySplitter] load: no value for" << keyPath();
            return;
        }

        const QByteArray ba = s.value(keyPath()).toByteArray();
        if (ba.isEmpty()) {
            qDebug() << "[MemorySplitter] load: empty QByteArray";
            return;
        }

        // десериализуем QByteArray обратно в QList<int>
        QDataStream in(ba);
        QList<int> sz;
        in >> sz;

        qDebug() << "[MemorySplitter] load: sizes =" << sz;
        if (!sz.isEmpty())
            setSizes(sz);
    }

    QString keyPath() const { return QString("widgets/splitter/%1").arg(m_key); }

    QString m_key;
};
