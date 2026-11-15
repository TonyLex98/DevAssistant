// MemoryLineEdit.h
#pragma once

#include <QLineEdit>
#include <QSettings>

class MemoryLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit MemoryLineEdit(const QString &key, QWidget *parent = nullptr)
        : QLineEdit(parent), m_key(key)
    {
        setStyleSheet(
            "QLineEdit {"
            "  border: 1px solid #0f766e;"
            "  border-radius: 6px;"
            "  padding: 6px 10px;"
            "  font: 10pt \"Segoe UI\";"
            "  background: #ffffff;"
            "  color: #0f172a;"
            "}"
            "QLineEdit:focus { border-color: #14b8a6; background: #ecfeff; }"
        );

        connect(this, &QLineEdit::editingFinished, this, &MemoryLineEdit::store);
        load();
    }

    void setKey(const QString &key) { m_key = key; }
    void loadState() { load(); }

    ~MemoryLineEdit() override
    {
        store();
    }

private:
    void store()
    {
        QSettings s;
        s.setValue(keyPath(), text());
    }

    void load()
    {
        QSettings s;
        if (!s.contains(keyPath()))
            return;

        const QString txt = s.value(keyPath()).toString();
        blockSignals(true);
        setText(txt);
        blockSignals(false);
    }

    QString keyPath() const
    {
        return QString("widgets/lineedit/%1").arg(m_key);
    }

    QString m_key;
};
