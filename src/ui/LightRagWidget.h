#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QProgressBar>
#include <QTableView>
#include <QHeaderView>
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QDialog>
#include <QMainWindow>
#include <QStandardItemModel>
#include <QTimer>

class LightRagClient;
class OllamaClient;

// Model for the file table in Indexation tab
class FileTableModel : public QStandardItemModel
{
    Q_OBJECT

public:
    enum Column {
        FilePath = 0,
        Status,
        LastIndexTime,
        ErrorMessage,
        ColumnCount
    };

    explicit FileTableModel(QObject *parent = nullptr);

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
};

class LightRagWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LightRagWidget(QWidget *parent = nullptr);
    ~LightRagWidget();

private slots:
    void saveSettings();
    void loadSettings();
    void browseIndexFolder();
    void browseStorageFolder();
    void startIndexation();
    void onIndexChanged(const QString &path);
    void onFileChanged(const QString &path);
    void updateModelIndexStatus(const QString &filePath, const QString &status, const QString &message = QString(), const QString &time = QString());
    void onModelsReceived(const QStringList &models);
    void onChatResponse(const QString &response);
    void onChatStreamResponse(const QString &response);
    void onChatError(const QString &error);
    void onIndexChangedStatus(const QString &filePath, bool success, const QString &message);
    void onIndexProgress(const QString &filePath, int progress);
    void updateAutoIndexCheckBox();
    void sendChatMessage();
    void showGraphWindow();

private:
    void setupUI();
    void setupSettingsTab();
    void setupIndexationTab();
    void setupChatTab();
    void setupGraphTab();
    void loadModels();
    void findFilesRecursively(const QString &dirPath);
    
    // Settings tab widgets
    QTabWidget *m_tabWidget;
    QWidget *m_settingsTab;
    QLineEdit *m_embeddingBindingEdit;
    QLineEdit *m_embeddingHostEdit;
    QLineEdit *m_embeddingModelEdit;
    QSpinBox *m_embeddingDimSpinBox;
    QSpinBox *m_timeoutSpinBox;
    QSpinBox *m_maxAsyncSpinBox;
    QLineEdit *m_llmModelEdit;
    QSpinBox *m_chunkSizeSpinBox;
    QSpinBox *m_chunkOverlapSizeSpinBox;
    QLineEdit *m_summaryLanguageEdit;
    QLineEdit *m_languageEdit;
    QLineEdit *m_indexFolderEdit;
    QPushButton *m_browseIndexFolderBtn;
    QLineEdit *m_storageFolderEdit;
    QPushButton *m_browseStorageFolderBtn;
    
    // Indexation tab widgets
    QWidget *m_indexationTab;
    FileTableModel *m_fileTableModel;
    QTableView *m_fileTableView;
    QPushButton *m_startIndexBtn;
    QProgressBar *m_progressBar;
    QCheckBox *m_autoIndexCheckBox;
    QFileSystemWatcher *m_fileWatcher;
    QStringList m_indexedFolders;
    
    // Chat tab widgets
    QWidget *m_chatTab;
    QTextBrowser *m_chatHistory;
    QPlainTextEdit *m_chatInput;
    QPushButton *m_sendBtn;
    QComboBox *m_modelCombo;
    OllamaClient *m_ollamaClient;
    
    // Graph tab widgets
    QWidget *m_graphTab;
    QPushButton *m_showGraphBtn;
    
    // Clients
    LightRagClient *m_lightRagClient;
    
    // Other
    QString m_configFilePath;
    bool m_indexingRunning;
    QStringList m_filesToIndex;
    QTimer *m_statusUpdateTimer;
    
    // For graph visualization
    QMainWindow *m_graphWindow;
};