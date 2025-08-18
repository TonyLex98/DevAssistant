#include "ChangesDialog.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QFile>

ChangesDialog::ChangesDialog(QWidget* parent):QDialog(parent){
    setWindowTitle("Confirm changes");
    resize(720,520);
    auto v = new QVBoxLayout(this);
    m_list = new QListWidget(this);
    v->addWidget(m_list,1);
    m_preview = new QTextEdit(this); m_preview->setReadOnly(true);
    v->addWidget(m_preview,1);
    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);
    v->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &ChangesDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &ChangesDialog::reject);
    connect(m_list, &QListWidget::currentRowChanged, this, &ChangesDialog::onItemChanged);
}

void ChangesDialog::setChanges(const QList<Change>& ch){
    m_changes = ch;
    m_list->clear();
    for (const Change& c : m_changes) {
        QString t;
        switch (c.type) {
            case Change::CreateFile: t = "CREATE " + c.targetPath; break;
            case Change::ReplaceInFile: t = "REPLACE " + c.targetPath; break;
            case Change::DeleteFile: t = "DELETE " + c.targetPath; break;
            case Change::RenameFile: t = "RENAME " + c.aux + " -> " + c.targetPath; break;
        }
        m_list->addItem(t);
    }
    if (!m_changes.isEmpty()) m_list->setCurrentRow(0);
}

void ChangesDialog::onItemChanged(int row){
    if (row<0 || row>=m_changes.size()) return;
    const Change& c = m_changes[row];
    QString prev, nxt;
    if (c.type==Change::CreateFile) { nxt = QString::fromUtf8(c.newContent); }
    else if (c.type==Change::ReplaceInFile) {
        QFile f(c.targetPath); if (f.open(QIODevice::ReadOnly)) { prev = QString::fromUtf8(f.readAll()); f.close(); }
        nxt = QString::fromUtf8(c.newContent);
    } else if (c.type==Change::DeleteFile) {
        QFile f(c.targetPath); if (f.open(QIODevice::ReadOnly)) { prev = QString::fromUtf8(f.readAll()); f.close(); }
    } else if (c.type==Change::RenameFile) {
        QFile f(c.aux); if (f.open(QIODevice::ReadOnly)) { prev = QString::fromUtf8(f.readAll()); f.close(); }
        QFile f2(c.targetPath); if (f2.open(QIODevice::ReadOnly)) { nxt = QString::fromUtf8(f2.readAll()); f2.close(); }
    }
    QString html = "<b>Before:</b><pre style='background:#300; color:#fff; padding:8px;'>"+prev.toHtmlEscaped()+"</pre>";
    html += "<b>After:</b><pre style='background:#030; color:#fff; padding:8px;'>"+nxt.toHtmlEscaped()+"</pre>";
    m_preview->setHtml(html);
}
