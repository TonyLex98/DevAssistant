#pragma once

#include <QComboBox>
#include <QSettings>
#include <QDebug>
#include <QTimer>

class MemoryComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit MemoryComboBox(const QString &key,
                           QWidget *parent = nullptr)
        : QComboBox(parent), m_key(key)
    {
        qDebug() << "[MemoryComboBox] ctor, key =" << m_key;

        /* ----------------- Стили ----------------- */
        setStyleSheet(
            "QComboBox {"
            "  border: 2px solid #5A9;"
            "  border-radius: 12px;"
            "  padding: 6px 12px;"
            "  font: 18px \"Arial\", sans-serif;"
            "}"
        );

        /* ----------------- Сигналы для автосохранения ---------------- */
        connect(this, &QComboBox::editTextChanged,
                this, &MemoryComboBox::store);

        // Load the saved value after construction, but only if items exist
        if (count() > 0) {
            load();
        } else {
            // Defer loading until items are added - connect to count change
            // Actually, we'll load at the end of the constructor after MainWindow has added items
            // For now, just schedule a single shot timer to load after items are added
            QTimer::singleShot(0, this, [this]() {
                if (count() > 0) {  // Only load if we have items now
                    load();
                }
            });
        }
    }

    /* ---------- Вставляем список и сразу восстанавливаем состояние ---------- */
    void setItems(const QStringList &list)
    {
        qDebug() << "[MemoryComboBox] setItems, count =" << list.size();
        clear();
        addItems(list);
        load();          // восстанавливаем выбранный пункт
    }

    /* Позволяет сменить ключ позднее, если понадобится */
    void setKey(const QString &key) { m_key = key; }

    /* Для тех, кто хочет вручную вызвать восстановление */
    void loadState() { load(); }

    /* ---------- Destructor – финальное сохранение ----------------------- */
    ~MemoryComboBox() override
    {
        qDebug() << "[MemoryComboBox] dtor, key =" << m_key;
        store();   // гарантируем, что последняя выборка записана
    }

private:
    /* ---------- Сохраняем в QSettings ---------------- */
    void store()
    {
        qDebug() << "[MemoryComboBox] store, text =" << currentText();
        QSettings s;
        s.setValue(keyPath(), currentText());
    }

    /* ---------- Загружаем из QSettings ---------------- */
    void load()
    {
        QSettings s;
        if (!s.contains(keyPath())) {
            qDebug() << "[MemoryComboBox] load: no value for" << keyPath();
            return;
        }

        const QString txt = s.value(keyPath()).toString();
        qDebug() << "[MemoryComboBox] load: value =" << txt;
        if (txt.isEmpty()) return;

        int idx = findText(txt, Qt::MatchExactly);
        if (idx >= 0) {
            qDebug() << "[MemoryComboBox] load: setting index =" << idx;
            setCurrentIndex(idx);          // это сработает store() ещё раз
        } else {
            qDebug() << "[MemoryComboBox] load: text not found in list";
        }
    }

    QString keyPath() const
    {
        return QString("widgets/combobox/%1").arg(m_key);
    }

    QString m_key;
};
