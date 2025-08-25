// RunSequenceDialog.cpp
#include "RunSequenceDialog.h"
#include <QVBoxLayout>
#include <QListWidgetItem>
#include <QDialogButtonBox>

RunSequenceDialog::RunSequenceDialog(const QMap<QString, PromptSequence> &seqs,
                                     QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Выбор последовательности");
    resize(500, 400);
    auto *v = new QVBoxLayout(this);
    m_list = new QListWidget(this);
    v->addWidget(m_list, 1);

    // Формируем видимый список: "<имя>  (путь/категория)"
    for (auto it = seqs.constBegin(); it != seqs.constEnd(); ++it) {
        const QString &path = it.key();
        const PromptSequence &seq = it.value();
        QString display = seq.name();
        if (!seq.description().isEmpty())
            display += " – " + seq.description();
        QListWidgetItem *item = new QListWidgetItem(display, m_list);
        item->setData(Qt::UserRole, path);
    }

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, this);
    v->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &RunSequenceDialog::onOk);
    connect(bb, &QDialogButtonBox::rejected, this, &RunSequenceDialog::reject);
}

void RunSequenceDialog::onOk()
{
    QListWidgetItem *cur = m_list->currentItem();
    if (!cur) { reject(); return; }
    m_selectedPath = cur->data(Qt::UserRole).toString();
    accept();
}
