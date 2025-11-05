// MemoryLineEdit.h
#pragma once

#include <QLineEdit>
#include <QSettings>
#include <QDebug>

class MemoryLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit MemoryLineEdit(const QString &key, QWidget *parent = nullptr)
        : QLineEdit(parent), m_key(key)
    {
        qDebug() << "[MemoryLineEdit] ctor, key =" << m_key;

        /* ---------- Стилизация (округлые углы, крупный шрифт) ---------- */
        setStyleSheet(
            "QLineEdit {"
            "  border: 2px solid #5A9;"
            "  border-radius: 12px;"
            "  padding: 6px 12px;"
            "  font: 18px \"Arial\", sans-serif;"
            "}"
        );

        /* ---------- Автосохранение при каждом изменении текста ---------- */
        connect(this, &QLineEdit::textChanged,
                this, &MemoryLineEdit::store);

        /* ---------- Загрузка сохранённого значения при создании ---------- */
        load();
    }

    /* Позволяет поменять ключ позже (если нужно) */
    void setKey(const QString &key) { m_key = key; }

    /* Вручную вызвать загрузку (если список виджетов уже существует) */
    void loadState() { load(); }

    ~MemoryLineEdit() override
    {
        qDebug() << "[MemoryLineEdit] dtor, key =" << m_key;
        store();           // финальное сохранение
    }

private:
    /* Сохраняем текст в QSettings */
    void store()
    {
        qDebug() << "[MemoryLineEdit] store, text =" << text();
        QSettings s;
        s.setValue(keyPath(), text());
    }

    /* Загружаем сохранённый текст (не генерируем сигнал) */
    void load()
    {
        QSettings s;
        if (!s.contains(keyPath())) {
            qDebug() << "[MemoryLineEdit] load: no value for" << keyPath();
            return;
        }

        const QString txt = s.value(keyPath()).toString();
        qDebug() << "[MemoryLineEdit] load, value =" << txt;

        blockSignals(true);   // чтобы не вызвать store() повторно
        setText(txt);
        blockSignals(false);
    }

    /* Путь в QSettings: widgets/lineedit/<key> */
    QString keyPath() const
    {
        return QString("widgets/lineedit/%1").arg(m_key);
    }

    QString m_key;
};
