#pragma once

#include <QWidget>
#include "SequenceRelationView.h"

class SequenceGraphWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SequenceGraphWidget(QWidget *parent = nullptr);
    
    void setSequenceManager(PromptSequenceManager *seqMgr);
    void setCurrentSequence(const QString &sequenceName);  // Highlight the current sequence

public slots:
    void rebuildGraph();
    void saveLayout();     // Save the current layout
    void restoreLayout();  // Restore the saved layout

signals:
    void sequenceSelected(const QString &sequenceName);

private:
    SequenceRelationView *m_view{nullptr};
};