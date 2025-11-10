#pragma once

/// @file RagWidget.h
/// @brief A simple Qt widget integrating the RAGProcessor.

#include "RAGProcessor.h"
#include <QWidget>
#include <QTreeView>
#include <QLabel>
#include <QGroupBox>
#include <QCheckBox>
#include "../src/ui/customWidgets/StyledButton.h"
#include "../src/ui/customWidgets/MemoryLineEdit.h"
#include "../src/ui/customWidgets/StyledProgressBar.h"
#include "../src/ui/customWidgets/StyledTextEdit.h"
#include "../src/adapters/OllamaAdapter.h"

namespace Rag
{
class RagWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RagWidget(QWidget *parent = nullptr);
    void setProjectPath(const QString &path);  // To set the project path for indexing
    void setMainOllamaAdapter(OllamaAdapter* adapter);  // To set the main Ollama adapter for summarization

private slots:
    void onIndexButtonClicked();
    void onAskButtonClicked();
    void onProgress(int processed, int total);
    void onIndexingFinished();
    void onAnswered(const QString &answer);
    void onStatusUpdate(const QString &status);
    
    // Search and summarization slots
    void onSearchButtonClicked();
    void onSearchResults(const QVector<Rag::Fragment> &results);
    void onSummarizeButtonClicked();

public:
    QTreeView *mTree;
    MemoryLineEdit *mRootEdit;
    StyledButton *mIndexBtn;
    MemoryLineEdit *mQueryEdit;
    StyledButton *mAskBtn;
    StyledButton *mSearchBtn;
    StyledButton *mSummarizeBtn;
    StyledProgressBar *mProgress;
    StyledTextEdit *mResult;
    QLabel *mStatusLabel;
    QCheckBox *mAutoUpdateCheckBox;
    RAGProcessor *mProcessor;
    OllamaAdapter* mMainOllamaAdapter = nullptr;  // Reference to main application's Ollama adapter
    QString mProjectPath;
    QVector<Rag::Fragment> mLastSearchResults;  // Store last search results for summarization
};

} // namespace Rag

