#include "LightRagWidget.h"
#include "LightRagClient.h"
#include "OllamaClient.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QTimer>
#include <QHeaderView>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QApplication>
#include "utils/Logging.h"

// FileTableModel implementation
FileTableModel::FileTableModel(QObject *parent)
    : QStandardItemModel(parent)
{
    setColumnCount(ColumnCount);
    setHorizontalHeaderLabels(QStringList() << "File Path" << "Status" << "Last Index Time" << "Error Message");
}

QVariant FileTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case FilePath: return QString("File Path");
        case Status: return QString("Status");
        case LastIndexTime: return QString("Last Index Time");
        case ErrorMessage: return QString("Error Message");
        default: return QVariant();
        }
    }
    return QStandardItemModel::headerData(section, orientation, role);
}

Qt::ItemFlags FileTableModel::flags(const QModelIndex &index) const
{
    if (index.column() == FilePath || index.column() == LastIndexTime || index.column() == ErrorMessage) {
        return QStandardItemModel::flags(index) & ~Qt::ItemIsEditable;
    }
    return QStandardItemModel::flags(index);
}

bool FileTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role == Qt::EditRole && (index.column() == Status)) {
        // Only allow editing status column
        return QStandardItemModel::setData(index, value, role);
    }
    return QStandardItemModel::setData(index, value, role);
}

// LightRagWidget implementation
LightRagWidget::LightRagWidget(QWidget *parent)
    : QWidget(parent)
    , m_tabWidget(new QTabWidget(this))
    , m_settingsTab(new QWidget())
    , m_indexationTab(new QWidget())
    , m_chatTab(new QWidget())
    , m_graphTab(new QWidget())
    , m_fileTableModel(new FileTableModel(this))
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_ollamaClient(new OllamaClient(this))
    , m_lightRagClient(new LightRagClient(this))
    , m_configFilePath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/lightrag_config.json")
    , m_indexingRunning(false)
    , m_statusUpdateTimer(new QTimer(this))
    , m_graphWindow(nullptr)
{
    // Ensure the config directory exists
    QDir configDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (!configDir.exists()) {
        configDir.mkpath(".");
    }
    
    setupUI();
    setupSettingsTab();
    setupIndexationTab();
    setupChatTab();
    setupGraphTab();
    
    // Add tabs to the widget
    m_tabWidget->addTab(m_settingsTab, "Настройки");
    m_tabWidget->addTab(m_indexationTab, "Индексация");
    m_tabWidget->addTab(m_chatTab, "Чат");
    m_tabWidget->addTab(m_graphTab, "Граф");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_tabWidget);
    setLayout(mainLayout);
    
    // Load saved settings
    loadSettings();
    
    // Connect signals
    connect(m_ollamaClient, &OllamaClient::modelsReceived, this, &LightRagWidget::onModelsReceived);
    connect(m_lightRagClient, &LightRagClient::chatResponse, this, &LightRagWidget::onChatResponse);
    connect(m_lightRagClient, &LightRagClient::chatStreamResponse, this, &LightRagWidget::onChatStreamResponse);
    connect(m_lightRagClient, &LightRagClient::chatError, this, &LightRagWidget::onChatError);
    connect(m_lightRagClient, &LightRagClient::indexProgress, this, &LightRagWidget::onIndexProgress);
    connect(m_lightRagClient, &LightRagClient::indexComplete, this, &LightRagWidget::onIndexChangedStatus);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, &LightRagWidget::onIndexChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &LightRagWidget::onFileChanged);
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &LightRagWidget::updateAutoIndexCheckBox);
    
    m_statusUpdateTimer->start(1000); // Update status every second
}

LightRagWidget::~LightRagWidget()
{
    if (m_graphWindow) {
        m_graphWindow->close();
        delete m_graphWindow;
    }
}

void LightRagWidget::setupUI()
{
    // Basic setup is done in constructor
}

void LightRagWidget::setupSettingsTab()
{
    QVBoxLayout *layout = new QVBoxLayout(m_settingsTab);
    
    QFormLayout *formLayout = new QFormLayout();
    
    // Create settings widgets
    m_embeddingBindingEdit = new QLineEdit("ollama");
    m_embeddingHostEdit = new QLineEdit("http://localhost:11434");
    m_embeddingModelEdit = new QLineEdit("qwen3-embedding:0.6b");
    m_embeddingDimSpinBox = new QSpinBox();
    m_embeddingDimSpinBox->setRange(1, 10000);
    m_embeddingDimSpinBox->setValue(2560);
    m_timeoutSpinBox = new QSpinBox();
    m_timeoutSpinBox->setRange(1, 3600);
    m_timeoutSpinBox->setValue(300);
    m_maxAsyncSpinBox = new QSpinBox();
    m_maxAsyncSpinBox->setRange(1, 100);
    m_maxAsyncSpinBox->setValue(1);
    m_llmModelEdit = new QLineEdit("gemma3:12b");
    m_chunkSizeSpinBox = new QSpinBox();
    m_chunkSizeSpinBox->setRange(1, 100000);
    m_chunkSizeSpinBox->setValue(1024);
    m_chunkOverlapSizeSpinBox = new QSpinBox();
    m_chunkOverlapSizeSpinBox->setRange(0, 10000);
    m_chunkOverlapSizeSpinBox->setValue(256);
    m_summaryLanguageEdit = new QLineEdit("Russian");
    m_languageEdit = new QLineEdit("ru");
    
    // Index and storage folders
    QHBoxLayout *indexFolderLayout = new QHBoxLayout();
    m_indexFolderEdit = new QLineEdit();
    m_browseIndexFolderBtn = new QPushButton("Browse...");
    indexFolderLayout->addWidget(m_indexFolderEdit);
    indexFolderLayout->addWidget(m_browseIndexFolderBtn);
    
    QHBoxLayout *storageFolderLayout = new QHBoxLayout();
    m_storageFolderEdit = new QLineEdit();
    m_browseStorageFolderBtn = new QPushButton("Browse...");
    storageFolderLayout->addWidget(m_storageFolderEdit);
    storageFolderLayout->addWidget(m_browseStorageFolderBtn);
    
    // Add widgets to form
    formLayout->addRow("EMBEDDING_BINDING:", m_embeddingBindingEdit);
    formLayout->addRow("EMBEDDING_BINDING_HOST:", m_embeddingHostEdit);
    formLayout->addRow("EMBEDDING_MODEL:", m_embeddingModelEdit);
    formLayout->addRow("EMBEDDING_DIM:", m_embeddingDimSpinBox);
    formLayout->addRow("TIMEOUT:", m_timeoutSpinBox);
    formLayout->addRow("MAX_ASYNC:", m_maxAsyncSpinBox);
    formLayout->addRow("LLM_MODEL:", m_llmModelEdit);
    formLayout->addRow("CHUNK_SIZE:", m_chunkSizeSpinBox);
    formLayout->addRow("CHUNK_OVERLAP_SIZE:", m_chunkOverlapSizeSpinBox);
    formLayout->addRow("SUMMARY_LANGUAGE:", m_summaryLanguageEdit);
    formLayout->addRow("LANGUAGE:", m_languageEdit);
    formLayout->addRow("Index Folder:", indexFolderLayout);
    formLayout->addRow("Storage Folder:", storageFolderLayout);
    
    layout->addLayout(formLayout);
    layout->addStretch();
    
    // Connect browse buttons
    connect(m_browseIndexFolderBtn, &QPushButton::clicked, this, &LightRagWidget::browseIndexFolder);
    connect(m_browseStorageFolderBtn, &QPushButton::clicked, this, &LightRagWidget::browseStorageFolder);
}

void LightRagWidget::setupIndexationTab()
{
    QVBoxLayout *layout = new QVBoxLayout(m_indexationTab);
    
    // Create table view
    m_fileTableView = new QTableView();
    m_fileTableView->setModel(m_fileTableModel);
    m_fileTableView->horizontalHeader()->setStretchLastSection(true);
    
    // Create controls
    QHBoxLayout *controlLayout = new QHBoxLayout();
    m_startIndexBtn = new QPushButton("Начать индексацию");
    m_progressBar = new QProgressBar();
    m_autoIndexCheckBox = new QCheckBox("Авто-переиндексация при изменении файлов");
    
    controlLayout->addWidget(m_startIndexBtn);
    controlLayout->addWidget(m_progressBar);
    controlLayout->addWidget(m_autoIndexCheckBox);
    
    layout->addWidget(m_fileTableView);
    layout->addLayout(controlLayout);
    
    // Connect signals
    connect(m_startIndexBtn, &QPushButton::clicked, this, &LightRagWidget::startIndexation);
}

void LightRagWidget::setupChatTab()
{
    QVBoxLayout *layout = new QVBoxLayout(m_chatTab);
    
    // Chat history
    m_chatHistory = new QTextBrowser();
    
    // Input area
    QHBoxLayout *inputLayout = new QHBoxLayout();
    m_chatInput = new QPlainTextEdit();
    m_sendBtn = new QPushButton("Отправить");
    m_modelCombo = new QComboBox();
    
    inputLayout->addWidget(m_modelCombo);
    inputLayout->addWidget(m_sendBtn);
    
    layout->addWidget(m_chatHistory);
    layout->addWidget(m_chatInput);
    layout->addLayout(inputLayout);
    
    // Connect signals
    connect(m_sendBtn, &QPushButton::clicked, this, &LightRagWidget::sendChatMessage);
    
    // Load models initially
    loadModels();
}

void LightRagWidget::setupGraphTab()
{
    QVBoxLayout *layout = new QVBoxLayout(m_graphTab);
    
    m_showGraphBtn = new QPushButton("Показать граф");
    
    layout->addWidget(m_showGraphBtn);
    layout->addStretch();
    
    connect(m_showGraphBtn, &QPushButton::clicked, this, &LightRagWidget::showGraphWindow);
}

void LightRagWidget::saveSettings()
{
    QJsonObject config;
    config["EMBEDDING_BINDING"] = m_embeddingBindingEdit->text();
    config["EMBEDDING_BINDING_HOST"] = m_embeddingHostEdit->text();
    config["EMBEDDING_MODEL"] = m_embeddingModelEdit->text();
    config["EMBEDDING_DIM"] = m_embeddingDimSpinBox->value();
    config["TIMEOUT"] = m_timeoutSpinBox->value();
    config["MAX_ASYNC"] = m_maxAsyncSpinBox->value();
    config["LLM_MODEL"] = m_llmModelEdit->text();
    config["CHUNK_SIZE"] = m_chunkSizeSpinBox->value();
    config["CHUNK_OVERLAP_SIZE"] = m_chunkOverlapSizeSpinBox->value();
    config["SUMMARY_LANGUAGE"] = m_summaryLanguageEdit->text();
    config["LANGUAGE"] = m_languageEdit->text();
    config["INDEX_FOLDER"] = m_indexFolderEdit->text();
    config["STORAGE_FOLDER"] = m_storageFolderEdit->text();
    config["AUTO_INDEX"] = m_autoIndexCheckBox->isChecked();
    
    QJsonDocument doc(config);
    QFile file(m_configFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    } else {
        qCWarning(logRag) << "Could not save config file:" << m_configFilePath;
    }
}

void LightRagWidget::loadSettings()
{
    QFile file(m_configFilePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject config = doc.object();
            m_embeddingBindingEdit->setText(config.value("EMBEDDING_BINDING").toString("ollama"));
            m_embeddingHostEdit->setText(config.value("EMBEDDING_BINDING_HOST").toString("http://localhost:11434"));
            m_embeddingModelEdit->setText(config.value("EMBEDDING_MODEL").toString("qwen3-embedding:0.6b"));
            m_embeddingDimSpinBox->setValue(config.value("EMBEDDING_DIM").toInt(2560));
            m_timeoutSpinBox->setValue(config.value("TIMEOUT").toInt(300));
            m_maxAsyncSpinBox->setValue(config.value("MAX_ASYNC").toInt(1));
            m_llmModelEdit->setText(config.value("LLM_MODEL").toString("gemma3:12b"));
            m_chunkSizeSpinBox->setValue(config.value("CHUNK_SIZE").toInt(1024));
            m_chunkOverlapSizeSpinBox->setValue(config.value("CHUNK_OVERLAP_SIZE").toInt(256));
            m_summaryLanguageEdit->setText(config.value("SUMMARY_LANGUAGE").toString("Russian"));
            m_languageEdit->setText(config.value("LANGUAGE").toString("ru"));
            m_indexFolderEdit->setText(config.value("INDEX_FOLDER").toString());
            m_storageFolderEdit->setText(config.value("STORAGE_FOLDER").toString());
            m_autoIndexCheckBox->setChecked(config.value("AUTO_INDEX").toBool(false));
        }
    } else {
        // Set defaults if config doesn't exist
        m_embeddingBindingEdit->setText("ollama");
        m_embeddingHostEdit->setText("http://localhost:11434");
        m_embeddingModelEdit->setText("qwen3-embedding:0.6b");
        m_embeddingDimSpinBox->setValue(2560);
        m_timeoutSpinBox->setValue(300);
        m_maxAsyncSpinBox->setValue(1);
        m_llmModelEdit->setText("gemma3:12b");
        m_chunkSizeSpinBox->setValue(1024);
        m_chunkOverlapSizeSpinBox->setValue(256);
        m_summaryLanguageEdit->setText("Russian");
        m_languageEdit->setText("ru");
    }
}

void LightRagWidget::browseIndexFolder()
{
    QString folder = QFileDialog::getExistingDirectory(this, "Select Index Folder", 
                                                      m_indexFolderEdit->text());
    if (!folder.isEmpty()) {
        m_indexFolderEdit->setText(folder);
        saveSettings();
    }
}

void LightRagWidget::browseStorageFolder()
{
    QString folder = QFileDialog::getExistingDirectory(this, "Select Storage Folder", 
                                                      m_storageFolderEdit->text());
    if (!folder.isEmpty()) {
        m_storageFolderEdit->setText(folder);
        saveSettings();
    }
}

void LightRagWidget::startIndexation()
{
    if (m_indexingRunning) {
        QMessageBox::information(this, "Indexation", "Indexation is already running.");
        return;
    }
    
    QString indexFolder = m_indexFolderEdit->text();
    if (indexFolder.isEmpty()) {
        QMessageBox::warning(this, "Indexation Error", "Please select an index folder first.");
        return;
    }
    
    // Prepare the lightRAG client with current settings
    m_lightRagClient->setSettings(
        m_embeddingHostEdit->text(),
        m_embeddingModelEdit->text(),
        m_llmModelEdit->text(),
        m_storageFolderEdit->text()
    );
    
    // Clear table and add files
    m_fileTableModel->setRowCount(0);
    m_filesToIndex.clear();

    // Recursively scan for all files in the directory and subdirectories
    findFilesRecursively(indexFolder);
    
    // Start indexation process
    m_indexingRunning = true;
    m_progressBar->setRange(0, m_filesToIndex.size());
    m_progressBar->setValue(0);
    
    // Update file watcher if auto-index is enabled
    if (m_autoIndexCheckBox->isChecked()) {
        m_indexedFolders << indexFolder;
        m_fileWatcher->addPath(indexFolder);
    }
    
    // Process each file
    for (const QString &filePath : m_filesToIndex) {
        m_lightRagClient->indexFile(filePath);
    }
}

void LightRagWidget::findFilesRecursively(const QString &dirPath)
{
    QDir dir(dirPath);
    // Get all files in this directory
    QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files, QDir::Name);

    // Add all files in this directory to the table and list
    for (const QFileInfo &info : fileInfoList) {
        int row = m_fileTableModel->rowCount();
        m_fileTableModel->insertRow(row);

        m_fileTableModel->setData(m_fileTableModel->index(row, FileTableModel::FilePath), info.absoluteFilePath(), Qt::EditRole);
        m_fileTableModel->setData(m_fileTableModel->index(row, FileTableModel::Status), "Pending", Qt::EditRole);
        m_fileTableModel->setData(m_fileTableModel->index(row, FileTableModel::LastIndexTime), "", Qt::EditRole);
        m_fileTableModel->setData(m_fileTableModel->index(row, FileTableModel::ErrorMessage), "", Qt::EditRole);

        m_filesToIndex << info.absoluteFilePath();
    }

    // Recursively process all subdirectories
    QFileInfoList dirInfoList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &dirInfo : dirInfoList) {
        findFilesRecursively(dirInfo.absoluteFilePath());
    }
}

void LightRagWidget::onIndexChanged(const QString &path)
{
    if (m_autoIndexCheckBox->isChecked()) {
        QDir dir(path);
        if (dir.exists()) {
            // Directory changed, re-index all files in it
            QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
            for (const QFileInfo &info : fileInfoList) {
                m_lightRagClient->indexFile(info.absoluteFilePath());
            }
        }
    }
}

void LightRagWidget::onFileChanged(const QString &path)
{
    if (m_autoIndexCheckBox->isChecked()) {
        // File changed, re-index it
        m_lightRagClient->indexFile(path);
    }
}

void LightRagWidget::updateModelIndexStatus(const QString &filePath, const QString &status, 
                                            const QString &message, const QString &time)
{
    for (int row = 0; row < m_fileTableModel->rowCount(); ++row) {
        QModelIndex index = m_fileTableModel->index(row, FileTableModel::FilePath);
        if (m_fileTableModel->data(index).toString() == filePath) {
            m_fileTableModel->setData(m_fileTableModel->index(row, FileTableModel::Status), status, Qt::EditRole);
            if (!time.isEmpty()) {
                m_fileTableModel->setData(m_fileTableModel->index(row, FileTableModel::LastIndexTime), time, Qt::EditRole);
            }
            if (!message.isEmpty()) {
                m_fileTableModel->setData(m_fileTableModel->index(row, FileTableModel::ErrorMessage), message, Qt::EditRole);
            }
            break;
        }
    }
}

void LightRagWidget::loadModels()
{
    m_ollamaClient->setHost(m_embeddingHostEdit->text());
    m_ollamaClient->getModels();
}

void LightRagWidget::onModelsReceived(const QStringList &models)
{
    m_modelCombo->clear();
    m_modelCombo->addItems(models);
    // Set the current model if it's in the list
    QString currentModel = m_llmModelEdit->text();
    int index = m_modelCombo->findText(currentModel);
    if (index >= 0) {
        m_modelCombo->setCurrentIndex(index);
    }
}

void LightRagWidget::onChatResponse(const QString &response)
{
    m_chatHistory->append("<b>Assistant:</b> " + response);
}

void LightRagWidget::onChatStreamResponse(const QString &response)
{
    // For streaming responses, we append to the current assistant response
    // This allows real-time updates as the response comes in
    QTextCursor cursor = m_chatHistory->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(response);
}

void LightRagWidget::onChatError(const QString &error)
{
    m_chatHistory->append("<b>Error:</b> " + error);
}

void LightRagWidget::onIndexChangedStatus(const QString &filePath, bool success, const QString &message)
{
    QString status = success ? "Indexed" : "Error";
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    updateModelIndexStatus(filePath, status, message, time);
    
    // Update progress
    int completed = 0;
    for (int row = 0; row < m_fileTableModel->rowCount(); ++row) {
        QModelIndex statusIndex = m_fileTableModel->index(row, FileTableModel::Status);
        QString statusText = m_fileTableModel->data(statusIndex).toString();
        if (statusText == "Indexed") {
            completed++;
        }
    }
    m_progressBar->setValue(completed);
    
    if (completed == m_filesToIndex.size()) {
        m_indexingRunning = false;
        QMessageBox::information(this, "Indexation Complete", "Indexation has completed.");
    }
}

void LightRagWidget::onIndexProgress(const QString &filePath, int progress)
{
    // Update the progress for the specific file if needed
    // For now, we'll just update the general progress bar
    Q_UNUSED(filePath);
    Q_UNUSED(progress);
}

void LightRagWidget::updateAutoIndexCheckBox()
{
    // Update the checkbox state if needed based on current status
    // This can be used to reflect the actual state of auto-indexing
}

void LightRagWidget::sendChatMessage()
{
    QString message = m_chatInput->toPlainText().trimmed();
    if (message.isEmpty()) {
        return;
    }
    
    // Add user message to history
    m_chatHistory->append("<b>User:</b> " + message);
    
    // Prepare the lightRAG client with current settings
    m_lightRagClient->setSettings(
        m_embeddingHostEdit->text(),
        m_embeddingModelEdit->text(),
        m_modelCombo->currentText(),
        m_storageFolderEdit->text()
    );
    
    // Use selected model from combobox if available, otherwise use the one from settings
    QString model = m_modelCombo->currentText();
    if (model.isEmpty()) {
        model = m_llmModelEdit->text();
    }
    
    m_lightRagClient->chatQuery(message, model);
    
    // Clear input
    m_chatInput->clear();
}

void LightRagWidget::showGraphWindow()
{
    if (!m_graphWindow) {
        m_graphWindow = new QMainWindow();
        m_graphWindow->setWindowTitle("LightRAG Knowledge Graph");
        m_graphWindow->resize(1200, 800);

        // Create a central widget with layout
        QWidget *centralWidget = new QWidget(m_graphWindow);
        QVBoxLayout *layout = new QVBoxLayout(centralWidget);

        // Add a refresh button
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        QPushButton *refreshBtn = new QPushButton("Refresh Graph");
        QPushButton *closeBtn = new QPushButton("Close");
        buttonLayout->addWidget(refreshBtn);
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeBtn);

        // Create web view for graph visualization
        QTextBrowser *graphView = new QTextBrowser();  // We'll use QTextBrowser for now as a placeholder
        // For now, we'll load some sample graph data or provide instructions
        graphView->setHtml("<h2>Knowledge Graph Visualization</h2>"
                          "<p>This would display the LightRAG knowledge graph visualization.</p>"
                          "<p>API endpoint for graph data: <code>" + m_embeddingHostEdit->text() + "/api/graph</code></p>"
                          "<p>To implement full graph visualization: </p>"
                          "<ul>"
                          "<li>Query the LightRAG API for graph data</li>"
                          "<li>Use D3.js or similar library for visualization</li>"
                          "<li>Display nodes (entities) and edges (relationships)</li>"
                          "</ul>");

        layout->addLayout(buttonLayout);
        layout->addWidget(graphView);

        m_graphWindow->setCentralWidget(centralWidget);

        // Connect buttons
        connect(refreshBtn, &QPushButton::clicked, [this, graphView]() {
            // In a real implementation, this would fetch graph data from API
            QString apiUrl = m_embeddingHostEdit->text() + "/api/graph";
            graphView->append("<br/>Refreshing graph from: " + apiUrl);
            graphView->append("Query parameters: index_path=" + m_storageFolderEdit->text());
        });

        connect(closeBtn, &QPushButton::clicked, m_graphWindow, &QMainWindow::close);
    }

    m_graphWindow->show();
    m_graphWindow->raise();
    m_graphWindow->activateWindow();
}