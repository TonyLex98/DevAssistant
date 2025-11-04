#pragma once

/// @file RagWidget.h
/// @brief A simple Qt widget integrating the RAGProcessor.

#include "RAGProcessor.h"
#include <QWidget>
#include <QTreeView>
#include "../src/ui/customWidgets/StyledButton.h"
#include "../src/ui/customWidgets/MemoryLineEdit.h"
#include "../src/ui/customWidgets/StyledProgressBar.h"
#include "../src/ui/customWidgets/StyledTextEdit.h"

namespace Rag
{
class RagWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RagWidget(QWidget *parent = nullptr);

private slots:
    void onIndexButtonClicked();
    void onAskButtonClicked();
    void onProgress(int processed, int total);
    void onIndexingFinished();
    void onAnswered(const QString &answer);

private:
    QTreeView *mTree;
    MemoryLineEdit *mRootEdit;
    StyledButton *mIndexBtn;
    MemoryLineEdit *mQueryEdit;
    StyledButton *mAskBtn;
    StyledProgressBar *mProgress;
    StyledTextEdit *mResult;
    RAGProcessor *mProcessor;
};

} // namespace Rag

