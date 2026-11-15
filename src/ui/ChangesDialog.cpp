#include "ChangesDialog.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QFile>

ChangesDialog::ChangesDialog(QWidget* parent):QDialog(parent){
    setWindowTitle(tr("Confirm changes"));
    resize(720,520);
    auto v = new QVBoxLayout(this);
    m_list = new StyledListWidget(this);
    v->addWidget(m_list,1);
    m_preview = new StyleTextEdit(this); m_preview->setReadOnly(true);
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
            case Change::CreateFile: t = tr("CREATE %1").arg(c.targetPath); break;
            case Change::ReplaceInFile: t = tr("REPLACE %1").arg(c.targetPath); break;
            case Change::DeleteFile: t = tr("DELETE %1").arg(c.targetPath); break;
            case Change::RenameFile: t = tr("RENAME %1 -> %2").arg(c.aux, c.targetPath); break;
            case Change::CreateDir: t = tr("MKDIR %1").arg(c.targetPath); break;
            case Change::DeleteDir: t = tr("RMDIR %1%2").arg(c.targetPath, c.recursive ? tr(" (recursive)") : QString()); break;
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
    } else if (c.type==Change::CreateDir) {
        nxt = tr("Created directory: %1").arg(c.targetPath);
    } else if (c.type==Change::DeleteDir) {
        prev = tr("Deleted directory: %1").arg(c.targetPath);
    }
    QString html = tr("<b>Before:</b>") + "<pre style='background:#300; color:#fff; padding:8px;'>"+prev.toHtmlEscaped()+"</pre>";
    html += tr("<b>After:</b>") + "<pre style='background:#030; color:#fff; padding:8px;'>"+nxt.toHtmlEscaped()+"</pre>";
    m_preview->setHtml(html);
}
