#include "RagWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QStandardItemModel>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>

namespace Rag
{

RagWidget::RagWidget(QWidget *parent) : QWidget(parent)
{
    mProcessor = new RAGProcessor(this);
    mProcessor->init();
    mTree = new QTreeView(this);
    mRootEdit = new MemoryLineEdit("rag_root_path", this);
    mIndexBtn = new StyledButton(tr("Index Project"), this);
    mQueryEdit = new MemoryLineEdit("rag_query", this);
    mAskBtn = new StyledButton(tr("Ask"), this);
    mSearchBtn = new StyledButton(tr("Search"), this);
    mSummarizeBtn = new StyledButton(tr("Summarize Results"), this);
    mProgress = new StyledProgressBar(this);
    mResult = new StyledTextEdit(this);
    mResult->setReadOnly(true);
    mStatusLabel = new QLabel(tr("Ready"), this);
    mAutoUpdateCheckBox = new QCheckBox(tr("Auto-update on file changes"), this);

    auto rootLayout = new QVBoxLayout(this);
    
    // Project path group
    auto projectGroup = new QGroupBox(tr("Project Settings"), this);
    auto projectLayout = new QVBoxLayout(projectGroup);
    auto topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel(tr("Project Path:"), this));
    topLayout->addWidget(mRootEdit);
    topLayout->addWidget(mIndexBtn);
    projectLayout->addLayout(topLayout);
    projectLayout->addWidget(mTree);
    projectLayout->addWidget(mAutoUpdateCheckBox);
    rootLayout->addWidget(projectGroup);
    
    // Query group
    auto queryGroup = new QGroupBox(tr("Query"), this);
    auto queryLayout = new QVBoxLayout(queryGroup);
    auto queryInputLayout = new QHBoxLayout();
    queryInputLayout->addWidget(new QLabel(tr("Query:"), this));
    queryInputLayout->addWidget(mQueryEdit);
    queryInputLayout->addWidget(mAskBtn);
    queryLayout->addLayout(queryInputLayout);
    
    // Search layout
    auto searchLayout = new QHBoxLayout();
    searchLayout->addWidget(new QLabel(tr("Search:"), this));
    searchLayout->addWidget(mQueryEdit); // Reuse the same query input
    searchLayout->addWidget(mSearchBtn);
    queryLayout->addLayout(searchLayout);
    
    // Summary layout
    auto summaryLayout = new QHBoxLayout();
    summaryLayout->addWidget(mSummarizeBtn);
    queryLayout->addLayout(summaryLayout);
    
    queryLayout->addWidget(mProgress);
    queryLayout->addWidget(new QLabel(tr("Status:"), this));
    queryLayout->addWidget(mStatusLabel);
    rootLayout->addWidget(queryGroup);
    
    // Results group
    auto resultsGroup = new QGroupBox(tr("Results"), this);
    auto resultsLayout = new QVBoxLayout(resultsGroup);
    resultsLayout->addWidget(mResult);
    rootLayout->addWidget(resultsGroup);
    
    setLayout(rootLayout);

    connect(mIndexBtn, &StyledButton::clicked, this, &RagWidget::onIndexButtonClicked);
    connect(mAskBtn, &StyledButton::clicked, this, &RagWidget::onAskButtonClicked);
    connect(mSearchBtn, &StyledButton::clicked, this, &RagWidget::onSearchButtonClicked);
    connect(mSummarizeBtn, &StyledButton::clicked, this, &RagWidget::onSummarizeButtonClicked);
    connect(mProcessor, &RAGProcessor::progress, this, &RagWidget::onProgress);
    connect(mProcessor, &RAGProcessor::indexingFinished, this, &RagWidget::onIndexingFinished);
    connect(mProcessor, &RAGProcessor::answered, this, &RagWidget::onAnswered);
    connect(mProcessor, &RAGProcessor::statusUpdate, this, &RagWidget::onStatusUpdate);
}

void RagWidget::setProjectPath(const QString &path)
{
    mProjectPath = path;
    mRootEdit->setText(path);
}

void RagWidget::setMainOllamaAdapter(OllamaAdapter* adapter)
{
    mMainOllamaAdapter = adapter;
    // Also initialize the processor with the main adapter
    if (mProcessor) {
        mProcessor->initWithOllamaAdapter(adapter);
    }
}

void RagWidget::onIndexButtonClicked()
{
    QString root;
    if (!mProjectPath.isEmpty()) {
        root = mProjectPath;  // Use the project path passed from main window
    } else {
        root = QFileDialog::getExistingDirectory(this, tr("Select Project Folder"));
        if (root.isEmpty()) return;
    }
    mRootEdit->setText(root);
    mProgress->setValue(0);
    mResult->append(tr("Starting indexing of: %1").arg(root));
    mProcessor->indexFolder(root);
}

void RagWidget::onAskButtonClicked()
{
    QString q = mQueryEdit->text();
    if (q.isEmpty()) return;
    mResult->append(tr("\nQuestion: %1\n").arg(q));
    mResult->append(tr("Processing..."));
    mProgress->setValue(0);
    mProcessor->ask(q);
}

void RagWidget::onProgress(int processed, int total)
{
    if (total > 0) {
        mProgress->setMaximum(total);
        mProgress->setValue(processed);
        mStatusLabel->setText(tr("Processing %1 of %2").arg(processed).arg(total));
    }
}

void RagWidget::onIndexingFinished()
{
    mProgress->setValue(mProgress->maximum());
    mResult->append(tr("\nIndexing completed."));
    mStatusLabel->setText(tr("Indexing completed"));
}

void RagWidget::onAnswered(const QString &answer)
{
    mResult->append(tr("\nAnswer: %1").arg(answer));
    mStatusLabel->setText(tr("Answer received"));
}

void RagWidget::onStatusUpdate(const QString &status)
{
    mStatusLabel->setText(status);
    mResult->append(tr("[Status] %1").arg(status));
}

void RagWidget::onSearchButtonClicked()
{
    QString q = mQueryEdit->text();
    if (q.isEmpty()) return;
    
    mResult->append(tr("\nSearching for: %1").arg(q));
    mProgress->setValue(0);
    
    // Perform search in a separate thread to keep UI responsive
    QMetaObject::invokeMethod(this, [this, q = q]() {
        QVector<Rag::Fragment> results = mProcessor->search(q, 10); // Get top 10 results
        
        // Store results for potential summarization
        mLastSearchResults = results;
        
        // Update UI in main thread
        QMetaObject::invokeMethod(this, [this, results]() {
            mResult->append(tr("\nSearch Results:"));
            for (int i = 0; i < results.size(); ++i) {
                const auto &fragment = results[i];
                mResult->append(tr("[%1] File: %2 (Line %3)")
                              .arg(i + 1)
                              .arg(QFileInfo(fragment.filePath).fileName())
                              .arg(fragment.paragraphNumber));
                mResult->append(tr("Content: %1\n").arg(fragment.text.left(200) + (fragment.text.length() > 200 ? "..." : "")));
            }
            mStatusLabel->setText(tr("Search completed. Found %1 results.").arg(results.size()));
        });
    });
}

void RagWidget::onSearchResults(const QVector<Rag::Fragment> &results)
{
    mResult->append(tr("\nSearch Results:"));
    for (int i = 0; i < results.size(); ++i) {
        const auto &fragment = results[i];
        mResult->append(tr("[%1] File: %2 (Line %3)")
                      .arg(i + 1)
                      .arg(QFileInfo(fragment.filePath).fileName())
                      .arg(fragment.paragraphNumber));
        mResult->append(tr("Content: %1\n").arg(fragment.text.left(200) + (fragment.text.length() > 200 ? "..." : "")));
    }
    mStatusLabel->setText(tr("Search completed. Found %1 results.").arg(results.size()));
}

void RagWidget::onSummarizeButtonClicked()
{
    if (mLastSearchResults.isEmpty()) {
        mResult->append(tr("\nNo search results to summarize. Please perform a search first."));
        return;
    }
    
    if (!mMainOllamaAdapter) {
        mResult->append(tr("\nMain Ollama adapter not set. Cannot perform summarization."));
        return;
    }
    
    mResult->append(tr("\nSummarizing search results..."));
    
    // Build context from all search results
    QString context;
    for (const auto &fragment : mLastSearchResults) {
        context += QString("File: %1 (Line %2)\n%3\n\n")
                      .arg(QFileInfo(fragment.filePath).fileName())
                      .arg(fragment.paragraphNumber)
                      .arg(fragment.text);
    }
    
    // Create a summarization query
    QString query = mQueryEdit->text();
    QString prompt = QString("Based on the following code/context snippets, provide a comprehensive summary that addresses the query: '%1'. "
                             "Focus on the most relevant information and provide key insights and code patterns that might be helpful.\n\n%2")
                      .arg(query)
                      .arg(context);
    
    // Use the main model for summarization 
    QMetaObject::invokeMethod(this, [this, prompt = prompt, context = context, query = query]() {
        // Use the main application's Ollama adapter for generating the summary
        QString summary = mMainOllamaAdapter->generateWithSystem(prompt, "You are a helpful AI assistant that summarizes code and technical content.");
        
        QMetaObject::invokeMethod(this, [this, summary = summary, query = query]() {
            mResult->append(tr("\nSUMMARY for query '%1':").arg(query));
            mResult->append(summary);
            mStatusLabel->setText(tr("Summarization completed."));
        });
    });
}

} // namespace Rag