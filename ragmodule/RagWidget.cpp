#include "RagWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QStandardItemModel>

namespace Rag
{

RagWidget::RagWidget(QWidget *parent) : QWidget(parent)
{
    mProcessor = new RAGProcessor(this);
    mProcessor->init();
    mTree = new QTreeView(this);
    mRootEdit = new MemoryLineEdit("rag_root_path", this);
    mIndexBtn = new StyledButton(tr("Index"), this);
    mQueryEdit = new MemoryLineEdit("rag_query", this);
    mAskBtn = new StyledButton(tr("Ask"), this);
    mProgress = new QProgressBar(this);
    mResult = new QTextEdit(this);
    mResult->setReadOnly(true);

    auto rootLayout = new QVBoxLayout(this);
    auto topLayout = new QHBoxLayout();
    topLayout->addWidget(mRootEdit);
    topLayout->addWidget(mIndexBtn);
    rootLayout->addLayout(topLayout);
    rootLayout->addWidget(mTree);
    auto queryLayout = new QHBoxLayout();
    queryLayout->addWidget(mQueryEdit);
    queryLayout->addWidget(mAskBtn);
    rootLayout->addLayout(queryLayout);
    rootLayout->addWidget(mProgress);
    rootLayout->addWidget(mResult);
    setLayout(rootLayout);

    connect(mIndexBtn, &StyledButton::clicked, this, &RagWidget::onIndexButtonClicked);
    connect(mAskBtn, &StyledButton::clicked, this, &RagWidget::onAskButtonClicked);
    connect(mProcessor, &RAGProcessor::progress, this, &RagWidget::onProgress);
    connect(mProcessor, &RAGProcessor::indexingFinished, this, &RagWidget::onIndexingFinished);
    connect(mProcessor, &RAGProcessor::answered, this, &RagWidget::onAnswered);
}

void RagWidget::onIndexButtonClicked()
{
    QString root = QFileDialog::getExistingDirectory(this, tr("Select Folder"));
    if (root.isEmpty()) return;
    mRootEdit->setText(root);
    mProgress->setValue(0);
    mProcessor->indexFolder(root);
}

void RagWidget::onAskButtonClicked()
{
    QString q = mQueryEdit->text();
    if (q.isEmpty()) return;
    mResult->clear();
    mProgress->setValue(0);
    mProcessor->ask(q);
}

void RagWidget::onProgress(int processed, int total)
{
    if (total > 0) {
        mProgress->setMaximum(total);
        mProgress->setValue(processed);
    }
}

void RagWidget::onIndexingFinished()
{
    mProgress->setValue(mProgress->maximum());
    mResult->append(tr("Indexing completed."));
}

void RagWidget::onAnswered(const QString &answer)
{
    mResult->append(answer);
}

} // namespace Rag
