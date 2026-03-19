#pragma once

#include <QByteArray>
#include <QDataStream>
#include <QSettings>
#include <QSplitter>

class MemorySplitter : public QSplitter
{
    Q_OBJECT
public:
    explicit MemorySplitter(const QString &key,
                            Qt::Orientation orientation = Qt::Horizontal,
                            QWidget *parent = nullptr)
        : QSplitter(orientation, parent), m_key(key)
    {
        setHandleWidth(4);
        setStyleSheet(
            "QSplitter {"
            "  border: none;"
            "  background: transparent;"
            "}"
            "QSplitter::handle {"
            "  border: none;"
            "  margin: 0px;"
            "  background: #33b5a6;"
            "}"
            "QSplitter::handle:horizontal {"
            "  height: 4px;"
            "}"
            "QSplitter::handle:vertical {"
            "  width: 4px;"
            "}"
            "QSplitter::handle:hover {"
            "  background: #1f9f90;"
            "}"
        );

        connect(this, &QSplitter::splitterMoved, this, &MemorySplitter::store);
    }

    void setKey(const QString &key) { m_key = key; }
    void loadState() { load(); }
    ~MemorySplitter() override { store(); }

private:
    void store()
    {
        const QList<int> sz = sizes();

        QByteArray ba;
        QDataStream out(&ba, QIODevice::WriteOnly);
        out << sz;

        QSettings s;
        s.setValue(keyPath(), ba);
        s.sync();
    }

    void load()
    {
        QSettings s;
        if (!s.contains(keyPath()))
            return;

        const QByteArray ba = s.value(keyPath()).toByteArray();
        if (ba.isEmpty())
            return;

        QDataStream in(ba);
        QList<int> sz;
        in >> sz;

        if (!sz.isEmpty())
            setSizes(sz);
    }

    QString keyPath() const { return QString("widgets/splitter/%1").arg(m_key); }

    QString m_key;
};
