#include "../utils/AppConfig.h"
#include "ModelsDialog.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>

ModelsDialog::ModelsDialog(OllamaAdapter* adapter, QWidget* parent):QDialog(parent),m_adapter(adapter){
    setWindowTitle("Models");
    resize(420,360);
    auto v = new QVBoxLayout(this);
    m_list = new QListWidget(this); v->addWidget(m_list,1);
    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);
    v->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &ModelsDialog::onAccept);
    connect(bb, &QDialogButtonBox::rejected, this, &ModelsDialog::reject);

    AppConfig cfg; cfg.load();
    QStringList models = m_adapter->listModels();
    if (models.isEmpty()) models << "llama3.1:8b-instruct" << "qwen2.5-coder:7b" << "codellama:7b-instruct";
    m_list->addItems(models);
    if (!cfg.selectedModel.isEmpty()) {
        QList<QListWidgetItem*> found = m_list->findItems(cfg.selectedModel, Qt::MatchExactly);
        if (!found.isEmpty()) m_list->setCurrentItem(found.first());
    }
}

void ModelsDialog::onAccept(){
    auto it = m_list->currentItem();
    if (!it) { reject(); return; }
    m_selected = it->text();
    AppConfig cfg2; cfg2.load();
    m_adapter->configure(m_selected, cfg2.params);
    cfg2.selectedModel = m_selected;
    cfg2.save();
    accept();
}
