#pragma once
#include <QDialog>
#include <QListWidget>
#include "../adapters/OllamaAdapter.h"

class ModelsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModelsDialog(OllamaAdapter* adapter, QWidget* parent=nullptr);
    QString selectedModel() const { return m_selected; }

private slots:
    void onAccept();

private:
    QListWidget* m_list=nullptr;
    QString m_selected;
    OllamaAdapter* m_adapter=nullptr;
};
