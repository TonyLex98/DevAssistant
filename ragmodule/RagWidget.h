#pragma once
#include "RagTypes.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTextBrowser;

namespace Rag {

class Index;

// "Documents" window: index a folder and ask questions about it.
class RagWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RagWidget(QWidget* parent = nullptr);

    // The folder offered by default: the open project.
    void setFolder(const QString& folder);

private:
    void startIndexing();
    void ask();
    void setBusy(bool busy);
    void showAnswer(const QString& answer, const QVector<Hit>& sources);
    void updateIndexInfo();

    Index* m_index = nullptr;
    QLineEdit* m_folder = nullptr;
    QLineEdit* m_embeddingModel = nullptr;
    QSpinBox* m_topK = nullptr;
    QPushButton* m_indexButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_status = nullptr;
    QLineEdit* m_question = nullptr;
    QPushButton* m_askButton = nullptr;
    QTextBrowser* m_answer = nullptr;
    QListWidget* m_sources = nullptr;
};

} // namespace Rag
