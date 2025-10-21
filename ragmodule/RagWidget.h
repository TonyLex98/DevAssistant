#pragma once

/// @file RagWidget.h
/// @brief A simple Qt widget integrating the RAGProcessor.

#include "RAGProcessor.h"
#include <QWidget>
#include <QTreeView>
#include <QProgressBar>
#include <QLineEdit>
#include <QTextEdit>

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
    QLineEdit *mRootEdit;
    QPushButton *mIndexBtn;
    QLineEdit *mQueryEdit;
    QPushButton *mAskBtn;
    QProgressBar *mProgress;
    QTextEdit *mResult;
    RAGProcessor *mProcessor;
};

} // namespace Rag

