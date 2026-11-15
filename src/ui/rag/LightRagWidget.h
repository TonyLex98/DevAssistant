#pragma once

#include <QJsonArray>
#include <QWidget>

class LightRagClient;
class QComboBox;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTextBrowser;

// Window for a LightRAG server: connect, upload documents and watch their processing, ask
// questions in LightRAG's retrieval modes, and explore the knowledge graph it builds.
class LightRagWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LightRagWidget(QWidget* parent = nullptr);

private:
    QWidget* buildDocumentsTab();
    QWidget* buildQueryTab();
    QWidget* buildGraphTab();

    void connectToServer();
    void uploadFiles(const QStringList& files);
    void showDocuments(const QJsonArray& documents);
    void showGraph(const QJsonArray& nodes, const QJsonArray& edges);
    void setStatus(const QString& text, bool error = false);

    LightRagClient* m_client = nullptr;

    QLineEdit* m_serverEdit = nullptr;
    QLineEdit* m_apiKeyEdit = nullptr;
    QLabel* m_status = nullptr;

    QTableWidget* m_documents = nullptr;
    QProgressBar* m_uploadProgress = nullptr;
    int m_uploadsPending = 0;

    QComboBox* m_mode = nullptr;
    QPlainTextEdit* m_question = nullptr;
    QPushButton* m_askButton = nullptr;
    QTextBrowser* m_answer = nullptr;

    QComboBox* m_labels = nullptr;
    QSpinBox* m_depth = nullptr;
    QGraphicsScene* m_scene = nullptr;
    QGraphicsView* m_view = nullptr;
};
