#pragma once

#include <QComboBox>
#include <QSettings>
#include <QTimer>
#include <QVariant>

class MemoryComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit MemoryComboBox(const QString &key,
                            QWidget *parent = nullptr)
        : QComboBox(parent), m_key(key)
    {
        setStyleSheet(
            "QComboBox {"
            "  border: 1px solid #0f766e;"
            "  border-radius: 6px;"
            "  padding: 6px 10px;"
            "  font: 10pt \"Segoe UI Variable\";"
            "  background: #ffffff;"
            "  color: #0f172a;"
            "}"
            "QComboBox:focus { border-color: #14b8a6; background: #ecfeff; }"
            "QComboBox QAbstractItemView {"
            "  selection-background-color: #99f6e4;"
            "  selection-color: #0f172a;"
            "}"
        );

        connect(this, QOverload<int>::of(&QComboBox::activated),
                this, [this](int) { store(); });

        connect(model(), &QAbstractItemModel::rowsInserted, this, [this]() { scheduleLoad(); });
        connect(model(), &QAbstractItemModel::modelReset, this, [this]() { scheduleLoad(); });
        scheduleLoad();
    }

    void setItems(const QStringList &list)
    {
        clear();
        addItems(list);
        scheduleLoad();
    }

    void setKey(const QString &key) { m_key = key; }
    void loadState() { load(); }

    ~MemoryComboBox() override
    {
        store();
    }

private:
    void scheduleLoad()
    {
        if (m_loadScheduled)
            return;
        m_loadScheduled = true;
        QTimer::singleShot(0, this, [this]() {
            m_loadScheduled = false;
            if (count() > 0)
                load();
        });
    }

    void store()
    {
        if (m_loading)
            return;

        QSettings s;
        s.setValue(keyPath(), currentText()); // backward compatible text key
        s.setValue(keyPathText(), currentText());
        s.setValue(keyPathData(), currentData());
    }

    void load()
    {
        QSettings s;

        // Prefer stable item data (path/id) when available.
        const QVariant savedData = s.value(keyPathData());
        const QString savedText = s.contains(keyPathText())
                                    ? s.value(keyPathText()).toString()
                                    : s.value(keyPath()).toString();

        int idx = -1;
        if (savedData.isValid() && !savedData.isNull()) {
            idx = findData(savedData, Qt::UserRole, Qt::MatchExactly);
        }
        if (idx < 0 && !savedText.isEmpty()) {
            idx = findText(savedText, Qt::MatchExactly);
        }
        if (idx >= 0 && idx < count()) {
            m_loading = true;
            setCurrentIndex(idx);
            m_loading = false;
        }
    }

    QString keyPath() const
    {
        return QString("widgets/combobox/%1").arg(m_key);
    }
    QString keyPathText() const { return keyPath() + "/text"; }
    QString keyPathData() const { return keyPath() + "/data"; }

    QString m_key;
    bool m_loadScheduled = false;
    bool m_loading = false;
};
