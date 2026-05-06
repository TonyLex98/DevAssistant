#include "MainWindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSet>
#include <QDateTime>
#include <QSizePolicy>

#include <QStatusBar>
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include <QTime>
#include <QLabel>
#include <QFrame>
#include "../ui/ChatDialog.h"
#include "../ui/ModelsDialog.h"
#include "../ui/OllamaSettingsDialog.h"
#include "../utils/AppConfig.h"
#include "../ui/PromptEditorDialog.h"
#include "../ui/ChangesDialog.h"
#include "../ui/RunTemplateOrSequenceDialog.h"
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTimer>
#include <QScrollArea>
#include <QStackedWidget>
#include <algorithm>
#include "../command/EditInRangeCommand.h"
#include "../command/RunShellCommand.h"
#include "../command/RunPythonCommand.h"
#include "../command/SearchWebCommand.h"
#include "../command/SearchProjectCommand.h"
#include "../command/ShowResultCommand.h"
#include "../command/Command.h"
#include "../command/CommandDocs.h"
#include "SolutionTemplateViewer.h"
#include "../core/SolutionTemplateManager.h"

static QList<QByteArray> splitConcatenatedJson_fromText(const QString& text){
    QByteArray raw = text.toUtf8();
    QList<QByteArray> parts;
    const char* s = raw.constData();
    int n = raw.size();
    bool in_str=false, esc=false;
    int depth=0, start=-1;
    for (int i=0;i<n;i++){
        char c = s[i];
        if (in_str){
            if (esc){ esc=false; continue; }
            if (c=='\\') { esc=true; continue; }
            if (c=='"') { in_str=false; continue; }
            continue;
        }
        if (c=='"'){ in_str=true; continue; }
        if (c=='{' || c=='['){
            if (depth==0) start=i;
            depth++;
        } else if (c=='}' || c==']'){
            depth--;
            if (depth==0 && start!=-1){
                parts.append(raw.mid(start, i-start+1));
                start=-1;
            }
        }
    }
    return parts;
}

static QString normalizeContextPath(const QString& path, const QString& projectRoot)
{
    if (path.trimmed().isEmpty())
        return QString();
    QFileInfo info(path);
    if (info.isAbsolute())
        return QDir::cleanPath(info.absoluteFilePath());
    return QDir(projectRoot).absoluteFilePath(path);
}

static QString displayContextPath(const QString& normalizedPath, const QString& projectRoot)
{
    QDir root(projectRoot);
    QString rel = root.relativeFilePath(normalizedPath);
    if (!rel.startsWith(".."))
        return QDir::toNativeSeparators(rel);
    return QDir::toNativeSeparators(normalizedPath);
}

static QVariantMap generationParamsFromConfig(const AppConfig& cfg)
{
    if (!cfg.params.isEmpty())
        return cfg.params;

    QVariantMap genParams;
    genParams["temperature"] = cfg.temperature;
    genParams["top_p"] = cfg.topP;
    genParams["top_k"] = cfg.topK;
    genParams["repeat_penalty"] = cfg.repeatPenalty;
    genParams["presence_penalty"] = cfg.presencePenalty;
    genParams["frequency_penalty"] = cfg.frequencyPenalty;
    genParams["num_predict"] = cfg.numPredict;
    genParams["num_ctx"] = cfg.numCtx;
    genParams["mirostat"] = cfg.mirostat;
    genParams["mirostat_tau"] = cfg.mirostatTau;
    genParams["mirostat_eta"] = cfg.mirostatEta;
    genParams["seed"] = cfg.seed;

    if (!cfg.stopWords.isEmpty()) {
        QJsonArray stopArray;
        for (const QString& word : cfg.stopWords)
            stopArray.append(word);
        genParams["stop"] = stopArray;
    }

    genParams["reasoning_effort"] = cfg.reasoningEffort;
    genParams["temperature_decay"] = cfg.temperatureDecay;
    genParams["repeat_last_n"] = cfg.repeatLastN;
    return genParams;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Initialize controllers
    m_statusController = new TaskStatusController(this);
    m_ttsController = new TtsController(this);
    m_sequenceRunner = new SequenceRunner(this);
    m_sequenceEditor = new SequenceEditorController(this);

    AppConfig startupCfg;
    startupCfg.load();
    const OllamaAdapter::Provider startupProvider =
        OllamaAdapter::providerFromString(startupCfg.provider);
    m_model.setProvider(startupProvider);
    m_agentModel.setProvider(startupProvider);

    QUrl startupEndpoint = startupCfg.endpoint;
    if (!startupEndpoint.isValid() || startupEndpoint.toString().trimmed().isEmpty())
        startupEndpoint = OllamaAdapter::defaultEndpointForProvider(startupProvider);
    m_model.setEndpoint(startupEndpoint);
    m_agentModel.setEndpoint(startupEndpoint);

    const QVariantMap startupParams = generationParamsFromConfig(startupCfg);
    const QString startupModel = startupCfg.selectedModel.trimmed();
    m_model.configure(startupModel, startupParams);
    m_agentModel.configure(startupModel, startupParams);

    // --------  1. Заголовок и корневой виджет ----------
    setWindowTitle("DevAssistantQt");

    // --------  2. IDE‑виджет (верхняя часть) ----------
    ide_widget = new IDEWidget("ideSplitter", Qt::Horizontal, this);          // <-- родитель `this`

    // --------  3. Вспомогательные виджеты (нижняя часть) ----------
    m_rootEdit  = new MemoryLineEdit("projectRoot", this);

    m_graphWindow = new GraphWidget();
    m_seqGraphWindow = nullptr;  // Will be created when needed
    browseBtn   = new StyledButton("Browse...", this);

    // 3.1   Верхняя строка (путь + Browse)
    QWidget * top_widget = new QWidget(this);
    auto top = new QHBoxLayout(top_widget);
    top->addWidget(m_rootEdit, 1);
    top->addWidget(browseBtn);

    // 3.2   Список файлов
    m_files = new QListWidget(this);

    // 3.3   Settings tab content — sidebar + pages
    m_settingsPanel = new QWidget(this);
    m_settingsPanel->setWindowTitle(tr("Settings"));
    m_settingsPanel->setAttribute(Qt::WA_DeleteOnClose, false);
    m_settingsPanel->setAttribute(Qt::WA_StyledBackground, true);
    m_settingsPanel->setObjectName("settingsPanel");
    m_settingsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* settingsRoot = new QHBoxLayout(m_settingsPanel);
    settingsRoot->setContentsMargins(12, 12, 12, 12);
    settingsRoot->setSpacing(12);

    // ---- Sidebar ----
    auto* sidebar = new QListWidget(this);
    sidebar->setMaximumWidth(180);
    sidebar->setMinimumWidth(140);
    sidebar->setStyleSheet(
        "QListWidget { background: #fff; border: 1px solid #8ed9cf; border-radius: 8px; padding: 6px; }"
        "QListWidget::item { padding: 8px 12px; border-radius: 6px; margin: 2px 0; font: 9pt \"Segoe UI Variable\"; color: #334155; }"
        "QListWidget::item:selected { background: #dff8f3; color: #0f766e; font-weight: 600; }"
        "QListWidget::item:hover { background: #e6f5f1; }"
    );
    sidebar->addItem("📁  Проект");
    sidebar->addItem("🤖  Модель");
    sidebar->addItem("📋  Последовательности");
    sidebar->addItem("📊  Графики");
    sidebar->addItem("🔊  Аудио");
    sidebar->addItem("⚙️  Система");
    settingsRoot->addWidget(sidebar, 0);

    // ---- Pages stack ----
    auto* pages = new QStackedWidget(this);
    pages->setStyleSheet("QStackedWidget { background: transparent; }");
    settingsRoot->addWidget(pages, 1);
    connect(sidebar, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    
    // Helper: scrollable page
    auto makePage = []() -> QWidget* {
        auto* w = new QWidget();
        w->setStyleSheet("QWidget { background: #f8fffd; }");
        w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        auto* l = new QVBoxLayout(w);
        l->setContentsMargins(12, 12, 12, 12);
        l->setSpacing(8);
        l->addStretch();
        return w;
    };
    auto pageLay = [](QWidget* w) -> QVBoxLayout* {
        return static_cast<QVBoxLayout*>(w->layout());
    };
    auto addSection = [](const QString& t, QVBoxLayout* l) {
        auto* lb = new QLabel(t);
        lb->setStyleSheet("QLabel { color: #0f766e; font: 600 11pt \"Segoe UI Variable\"; padding: 4px 0; }");
        l->addWidget(lb);
    };
    auto addDiv = [](QVBoxLayout* l) {
        auto* ln = new QFrame();
        ln->setFrameShape(QFrame::HLine);
        ln->setStyleSheet("QFrame { background: #bdebe4; min-height: 1px; max-height: 1px; border: none; }");
        l->addWidget(ln);
    };

    // -- Page 0: Project --
    auto* p0 = makePage();
    pages->addWidget(p0);
    auto* projLay = pageLay(p0);
    addSection(tr("Путь к проекту"), projLay);
    projLay->addWidget(top_widget);
    addDiv(projLay);
    addSection(tr("Действия"), projLay);
    rescanBtn = new StyledButton(tr("Обновить проект"), this);
    descBtn = new StyledButton(tr("Сгенерировать описания"), this);
    rescanBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    descBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    projLay->addWidget(rescanBtn);
    projLay->addWidget(descBtn);
    projLay->addStretch();

    // -- Page 1: Model --
    auto* p1 = makePage();
    pages->addWidget(p1);
    auto* llmLay = pageLay(p1);
    addSection(tr("Настройки модели"), llmLay);
    settingsBtn = new StyledButton(tr("Открыть настройки LLM"), this);
    promptBtn = new StyledButton(tr("Редактировать базовый промпт"), this);
    llmLay->addWidget(settingsBtn);
    llmLay->addWidget(promptBtn);
    addDiv(llmLay);
    m_llmSettingsWindow = new OllamaSettingsDialog(this);
    m_llmSettingsWindow->setVisible(false);
    llmLay->addWidget(m_llmSettingsWindow);
    llmLay->addStretch();

    // -- Page 2: Sequences --
    auto* p2 = makePage();
    pages->addWidget(p2);
    auto* seqLay = pageLay(p2);
    addSection(tr("Последовательности"), seqLay);
    editSeqBtn = new StyledButton(tr("Редактировать"), this);
    newSeqBtn = new StyledButton(tr("Создать"), this);
    runTemplateBtn = new StyledButton(tr("Запустить шаблон"), this);
    editTemplatesBtn = new StyledButton(tr("Редактировать шаблоны"), this);
    exportSequenceAsTemplateBtn = new StyledButton(tr("Экспорт как шаблон"), this);
    seqLay->addWidget(editSeqBtn);
    seqLay->addWidget(newSeqBtn);
    seqLay->addWidget(runTemplateBtn);
    seqLay->addWidget(editTemplatesBtn);
    seqLay->addWidget(exportSequenceAsTemplateBtn);
    seqLay->addStretch();

    // -- Page 3: Graphs --
    auto* p3 = makePage();
    pages->addWidget(p3);
    auto* graphLay = pageLay(p3);
    addSection(tr("Визуализация"), graphLay);
    graphBtn = new StyledButton(tr("Граф контекста"), this);
    seqGraphBtn = new StyledButton(tr("Граф последовательностей"), this);
    lightRagBtn = new StyledButton(tr("LightRAG"), this);
    graphLay->addWidget(graphBtn);
    graphLay->addWidget(seqGraphBtn);
    graphLay->addWidget(lightRagBtn);
    graphLay->addStretch();
    connect(graphBtn, &StyledButton::clicked, this, &MainWindow::openGraph);
    connect(seqGraphBtn, &StyledButton::clicked, this, &MainWindow::openSequenceGraph);
    connect(lightRagBtn, &StyledButton::clicked, this, &MainWindow::openLightRag);

    // -- Page 4: Audio --
    auto* p4 = makePage();
    pages->addWidget(p4);
    auto* audioLay = pageLay(p4);
    addSection(tr("Синтез речи (TTS)"), audioLay);
    // Use TtsController for TTS management
    audioLay->addWidget(m_ttsController->toggleButton());
    audioLay->addStretch();
    connect(m_ttsController, &TtsController::statusMessage, this, [this](const QString& m){ log("TTS: "+m); });
    connect(m_ttsController, &TtsController::errorMessage, this, [this](const QString& e){ log("TTS Error: "+e); });

    // -- Page 5: System --
    auto* p5 = makePage();
    pages->addWidget(p5);
    auto* sysLay = pageLay(p5);
    addSection(tr("Безопасность"), sysLay);
    confirmModeCombo = new MemoryComboBox("confirmMode", this);
    confirmModeCombo->addItem("Автоподтверждение команд", "auto");
    confirmModeCombo->addItem("Спрашивать подтверждение", "ask");
    confirmModeCombo->setCurrentIndex(0);
    sysLay->addWidget(confirmModeCombo);
    addDiv(sysLay);
    addSection(tr("Интерфейс"), sysLay);
    chatToggleBtn = new StyledButton(tr("Показать чат"), this);
    logToggleBtn = new StyledButton(tr("Показать лог"), this);
    chatToggleBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    logToggleBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sysLay->addWidget(chatToggleBtn);
    sysLay->addWidget(logToggleBtn);
    sysLay->addStretch();
   // Set Expanding policy for all buttons on settings pages so they stretch to fill width
    for (int i = 0; i < pages->count(); ++i) {
        auto* page = qobject_cast<QWidget*>(pages->widget(i));
        if (page) {
            for (auto* btn : page->findChildren<StyledButton*>())
                btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
    }

    // Hidden buttons
    applyBtn = new StyledButton("Apply", this);
    undoBtn = new StyledButton("Undo", this);
    applyBtn->hide();
    undoBtn->hide();
    runSeqBtn = new StyledButton(tr("Запустить"), this);
    runSeqBtn->hide();

    connect(newSeqBtn, &StyledButton::clicked, this, &MainWindow::createNewSequence);

    // -- Connections for page buttons --
    connect(runTemplateBtn, &StyledButton::clicked, this, [this]() {
        refreshAllSequences(); ensureDefaultSequence();
        RunTemplateOrSequenceDialog dlg(m_seqMgr.sequences(), &m_templateMgr, this);
        dlg.setTemplateModeOnly();
        if (dlg.exec() != QDialog::Accepted) return;
        if (dlg.selectedMode() != RunTemplateOrSequenceDialog::RunTemplate) {
            QMessageBox::warning(this, "Ошибка", "Необходимо выбрать шаблон."); return;
        }
        QString sp = dlg.selectedDefaultSequencePath();
        if (sp.isEmpty() || !QFile::exists(sp)) {
            QMessageBox::warning(this, "Ошибка", "Последовательность не найдена."); return;
        }
        PromptSequence seq; QFile sf(sp);
        if (sf.open(QIODevice::ReadOnly)) seq = PromptSequence::fromJson(QJsonDocument::fromJson(sf.readAll()).object());
        if (seq.name().isEmpty()) { QMessageBox::warning(this, "Ошибка", "Не удалось загрузить."); return; }
        m_currentSeq = seq; m_currentStepIdx = 0; m_sequenceMode = true;
        updateSequenceGraphHighlight(); runNextStep();
    });

    connect(editTemplatesBtn, &StyledButton::clicked, this, [this]() {
        // Create and show the template viewer
        SolutionTemplateViewer *viewer = new SolutionTemplateViewer(&m_templateMgr, this);
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Solution Templates"));
        dialog.resize(800, 600);

        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        layout->addWidget(viewer);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttonBox);

        dialog.exec();
    });

    connect(exportSequenceAsTemplateBtn, &StyledButton::clicked, this, [this]() {
        // First, refresh sequence manager
        refreshAllSequences();

        // Get list of sequences
        const QMap<QString, PromptSequence>& sequences = m_seqMgr.sequences();
        if (sequences.isEmpty()) {
            QMessageBox::information(this, "No Sequences", "No sequences found to export as templates.");
            return;
        }

        // Let user choose a sequence
        QStringList names;
        QMap<QString, QString> pathByName;      // name → path
        for (const QString &filePath : m_seqMgr.fileList()) {
            QString name = QFileInfo(filePath).fileName();
            names << name;
            pathByName.insert(name, filePath);
        }

        bool ok = false;
        QString selectedName = QInputDialog::getItem(
                    this, tr("Export Sequence as Template"),
                    tr("Choose sequence to export:"), names, 0, false, &ok);

        if (!ok || selectedName.isEmpty())
            return;          // пользователь отменил

        QString sequencePath = pathByName.value(selectedName);
        PromptSequence seq = sequences.value(sequencePath);

        // Ask for template name and description
        QString templateName = QInputDialog::getText(this, "Template Name", "Enter template name:",
            QLineEdit::Normal, seq.name(), &ok);
        if (!ok || templateName.trimmed().isEmpty()) {
            return;
        }

        QString description = QInputDialog::getMultiLineText(this, "Template Description",
            "Enter template description (optional):", seq.description(), &ok);
        if (!ok) { // User cancelled, allow empty description
            description = seq.description();
        }

        // Create the template directory
        QString templateDir = QDir(m_templateMgr.root()).absoluteFilePath(templateName.trimmed());
        if (QDir().exists(templateDir)) {
            QMessageBox::warning(this, "Import Failed",
                QString("Template with name '%1' already exists. Please choose a different name.").arg(templateName));
            return;
        }

        // Export the sequence as a template
        if (m_templateMgr.exportSequenceAsTemplate(sequencePath, templateDir, templateName.trimmed(), description)) {
            m_templateMgr.refresh();
            QMessageBox::information(this, "Export Successful",
                QString("Sequence '%1' exported as template '%2' successfully!").arg(
                    QFileInfo(sequencePath).fileName(), templateName));
        } else {
            QMessageBox::warning(this, "Export Failed",
                "Failed to export sequence as template. Check permissions and try again.");
        }
    });

    promptBtn->hide();
    applyBtn->hide();
    undoBtn->hide();

    // 3.4   Логи и чат
    m_log = new StyleTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setVisible(false);

    m_chat = new ChatDialog(&m_model, this);
    // Set the MainWindow reference in the ChatDialog so it can signal template stops
    m_chat->setMainWindow(this);

    // Initialize sequence graph widget so it can be embedded in the layout
    m_seqGraphWindow = new SequenceGraphWidget(this);
    m_seqGraphWindow->setSequenceManager(&m_seqMgr);
    m_seqGraphWindow->setTemplateManager(&m_templateMgr);
    m_seqGraphWindow->restoreLayout();
    m_seqGraphWindow->rebuildGraph();
    updateSequenceGraphTemplateSelection();
    m_seqGraphWindow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_seqGraphWindow->setVisible(true);
    connect(m_seqGraphWindow, &SequenceGraphWidget::sequenceSelected, this,
            [this](const QString &sequenceName) {
                openSequenceEditorForName(sequenceName);
            });

    connect(chatToggleBtn, &StyledButton::clicked, this, [this]() {
        const bool shouldShow = !m_chat->isChatLogVisible();
        m_chat->setChatLogVisible(shouldShow);
        chatToggleBtn->setText(shouldShow ? tr("Hide chat") : tr("Show chat"));
    });
    connect(logToggleBtn, &StyledButton::clicked, this, [this]() {
        const bool shouldShow = !m_log->isVisible();
        m_log->setVisible(shouldShow);
        logToggleBtn->setText(shouldShow ? tr("Hide log") : tr("Show log"));
    });

    for (StyledButton *btn : m_settingsPanel->findChildren<StyledButton*>()) {
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setMinimumHeight(32);
    }

    // 3.5 Result display widget
    m_resultDisplayWidget = new ResultDisplayWidget(this);

    // 3.5   (Опционально) виджет распознавания голоса
    m_voskWidget = new VoskWidget(this);
    connect(m_voskWidget, &VoskWidget::voiceRecognized,
            this, &MainWindow::onVoskText);





    // Initialize context status widget to display command context state
    m_contextStatusWidget = new QWidget(this);
    m_contextCommandsList = new QTreeWidget(m_contextStatusWidget);
    m_contextCommandsList->setObjectName("contextCommandsList");
    m_contextGraphWidget = new ContextGraphWidget(m_contextStatusWidget);
    m_contextTabs = new QTabWidget(m_contextStatusWidget);
    m_contextTabs->setDocumentMode(true);

    QVBoxLayout* contextLayout = new QVBoxLayout(m_contextStatusWidget);
    contextLayout->setContentsMargins(5, 5, 5, 5);

    // Create header without toggle button
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel(tr("Context Commands:"), m_contextStatusWidget);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    contextLayout->addLayout(headerLayout);
    m_contextTabs->addTab(m_contextCommandsList, tr("Commands"));
    m_contextTabs->addTab(m_contextGraphWidget, tr("Context Graph"));
    contextLayout->addWidget(m_contextTabs);

    // Set up the tree widget
    m_contextCommandsList->setHeaderLabels(QStringList() << "Time" << "Description");
    m_contextCommandsList->setColumnCount(2);
    m_contextCommandsList->setHeaderHidden(false);  // Show the header for better UX
    m_contextCommandsList->setRootIsDecorated(true); // Show expand/collapse icons
    m_contextCommandsList->setColumnWidth(0, 80);  // Set reasonable width for time column

    // Set size preference for the context status widget
    m_contextStatusWidget->setMinimumWidth(260);
    m_contextStatusWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    // Set the MainWindow reference in the ChatDialog so it can signal template stops
    m_chat->setMainWindow(this);

    // --------  4. Верхняя часть layout (внутри rightPanel) ----------
    /*QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addLayout(top);
    rightLayout->addWidget(m_files, 1);
    rightLayout->addLayout(btns);
    rightLayout->addWidget(m_log, 1);
    rightLayout->addWidget(m_chat, 1);*/
    // Settings panel is its own scrollable container (pages have inner scroll)
    m_settingsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // --------  5. Формируем layout: слева context, по центру основной контент, справа graph ----------
    m_centerSplitter = new MemorySplitter("MainWindowsCenterSplitter", Qt::Vertical, this);
    m_centerSplitter->addWidget(ide_widget);
    m_centerSplitter->addWidget(m_log);
    m_centerSplitter->addWidget(m_resultDisplayWidget);
    m_centerSplitter->addWidget(m_chat);

    mainSplitter = new MemorySplitter("MainWindowsMainSplitter", Qt::Horizontal, this);
    mainSplitter->addWidget(m_contextStatusWidget);
    mainSplitter->addWidget(m_centerSplitter);
    mainSplitter->addWidget(m_seqGraphWindow);

    m_centerSplitter->setStretchFactor(0, 5);
    m_centerSplitter->setStretchFactor(1, 1);
    m_centerSplitter->setStretchFactor(2, 3);
    m_centerSplitter->setStretchFactor(3, 2);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 3);
    mainSplitter->setStretchFactor(2, 2);

    m_files->hide();



    //QWidget *rightPanel = new QWidget(this);
    //rightPanel->setLayout(rightLayout);    // нижняя часть
    //mainSplitter->addWidget(rightPanel);

    QTabWidget *rootTabs = new QTabWidget(this);
    rootTabs->addTab(mainSplitter, tr("Assistant"));
    rootTabs->addTab(m_settingsPanel, tr("Settings"));

    // Задаём начальные размеры (опционально)
    setCentralWidget(rootTabs);

    // --------  6. Соединяем сигналы / слоты ----------
    connect(browseBtn,   &StyledButton::clicked, this, &MainWindow::browse);
    connect(rescanBtn,   &StyledButton::clicked, this, &MainWindow::rescan);
    connect(settingsBtn, &StyledButton::clicked, this, &MainWindow::openSettings);
    connect(promptBtn,   &StyledButton::clicked, this, &MainWindow::editBasePrompt);
    connect(applyBtn,    &StyledButton::clicked, this, &MainWindow::applyChanges);
    connect(undoBtn,     &StyledButton::clicked, this, &MainWindow::undoLast);
    connect(m_llmSettingsWindow, &OllamaSettingsDialog::saveRequested, this, [this]() {
        if (!m_llmSettingsWindow)
            return;

        const OllamaAdapter::Provider provider =
            OllamaAdapter::providerFromString(m_llmSettingsWindow->provider());
        const bool providerChanged = (m_model.provider() != provider);

        m_model.setProvider(provider);
        m_agentModel.setProvider(provider);

        QUrl endpoint(m_llmSettingsWindow->endpoint().trimmed());
        if (!endpoint.isValid() || endpoint.toString().trimmed().isEmpty())
            endpoint = OllamaAdapter::defaultEndpointForProvider(provider);
        m_model.setEndpoint(endpoint);
        m_agentModel.setEndpoint(endpoint);

        const QVariantMap opts = m_llmSettingsWindow->params();
        const QString selectedModel = m_model.selectedModel().trimmed();
        const QString selectedAgentModel = m_agentModel.selectedModel().trimmed();
        if (!selectedModel.isEmpty())
            m_model.configure(selectedModel, opts);
        if (!selectedAgentModel.isEmpty())
            m_agentModel.configure(selectedAgentModel, opts);

        AppConfig cfg;
        cfg.load();
        cfg.provider = OllamaAdapter::providerToString(provider);
        cfg.endpoint = endpoint;
        cfg.params = opts;
        if (providerChanged)
            cfg.selectedModel.clear();

        cfg.temperature = opts.value("temperature", 1).toDouble();
        cfg.topP = opts.value("top_p", 0.9).toDouble();
        cfg.topK = opts.value("top_k", 40).toInt();
        cfg.repeatPenalty = opts.value("repeat_penalty", 1.1).toDouble();
        cfg.presencePenalty = opts.value("presence_penalty", 0.0).toDouble();
        cfg.frequencyPenalty = opts.value("frequency_penalty", 0.0).toDouble();
        cfg.numPredict = opts.value("num_predict", -1).toInt();
        cfg.numCtx = opts.value("num_ctx", 4096).toInt();
        cfg.mirostat = opts.value("mirostat", 0).toInt();
        cfg.mirostatTau = opts.value("mirostat_tau", 5.0).toDouble();
        cfg.mirostatEta = opts.value("mirostat_eta", 0.1).toDouble();
        cfg.seed = opts.value("seed", 0).toULongLong();

        QStringList stopWords;
        if (opts.contains("stop")) {
            const QJsonArray stopArray = opts.value("stop").toJsonArray();
            for (const QJsonValue& val : stopArray) {
                const QString word = val.toString().trimmed();
                if (!word.isEmpty())
                    stopWords << word;
            }
        }
        cfg.stopWords = stopWords;

        cfg.reasoningEffort = opts.value("reasoning_effort", "medium").toString();
        cfg.temperatureDecay = opts.value("temperature_decay", 0.0).toDouble();
        cfg.repeatLastN = opts.value("repeat_last_n", 64).toInt();
        cfg.save();

        if (m_chat)
            m_chat->refreshModelList();

        statusBar()->showMessage(QString("Provider: %1 | Model: %2")
                                     .arg(m_model.providerName(),
                                          m_model.selectedModel().trimmed().isEmpty()
                                              ? tr("<not selected>")
                                              : m_model.selectedModel().trimmed()));
        log(QString("LLM settings applied: provider=%1 endpoint=%2")
                .arg(m_model.providerName(), endpoint.toString()));
    });

    // Chat + модель
    connect(m_chat, &ChatDialog::sendMessage, this, &MainWindow::sendUserMessage);
    connect(m_chat, &ChatDialog::sendMessageWithImages, this, &MainWindow::sendUserMessageWithImages);
    connect(m_chat, &ChatDialog::runTemplateRequested, this, &MainWindow::runTemplateRequested);
    connect(m_chat, &ChatDialog::runTemplateRequestedWithImages, this, &MainWindow::runTemplateRequestedWithImages);
    connect(m_chat, &ChatDialog::runSimpleChat, this, &MainWindow::runSimpleChat);
    connect(m_chat, &ChatDialog::runSimpleChatWithImages, this, &MainWindow::runSimpleChatWithImages);
    connect(m_chat, &ChatDialog::templateSelectionChanged, this, &MainWindow::onChatTemplateSelectionChanged);
    connect(&m_model, &OllamaAdapter::streamChunk, this, &MainWindow::onModelChunk);
    connect(&m_model, &OllamaAdapter::streamFinished, this, &MainWindow::handleAssistantFinished);
    connect(&m_model, &OllamaAdapter::streamError,
            this, [this](const QString &e){ log("Model error: " + e); });

    connect(runSeqBtn, &StyledButton::clicked, this, &MainWindow::runSequenceDialog);
    connect(editSeqBtn, &StyledButton::clicked, this, &MainWindow::editSequences);

    connect(descBtn, &StyledButton::clicked, this, &MainWindow::generateDescriptions);

    // Состояния
    statusBar()->showMessage(QString("Provider: %1 | Model: %2")
                                 .arg(m_model.providerName(),
                                      m_model.selectedModel().trimmed().isEmpty()
                                          ? tr("<not selected>")
                                          : m_model.selectedModel().trimmed()));

    m_projectRoot = m_rootEdit->text();



    // Папка, где будем хранить наборы
    QString seqRoot = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                        .append("/DevelopersAssistant/PromptSequences");

    // Создаём каталог, если его нет
    QDir seqDir(seqRoot);
    if (!seqDir.exists())
        seqDir.mkpath(".");

    m_seqMgr.setRoot(seqRoot);
    refreshAllSequences();
    ensureDefaultSequence();

    // Папка, где будем хранить шаблоны решений
    QString tmplRoot = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                        .append("/DevelopersAssistant/SolutionTemplates");

    // Создаём каталог, если его нет
    QDir tmplDir(tmplRoot);
    if (!tmplDir.exists())
        tmplDir.mkpath(".");

    m_templateMgr.setRoot(tmplRoot);
    m_templateMgr.setSequenceManager(&m_seqMgr);  // Connect sequence manager to template manager
    m_templateMgr.refresh();

    auto ensureUniversalAgentTemplate = [this]() {
        const QString templateDir = QDir(m_templateMgr.root()).filePath("UniversalAgent");
        const QString versionFilePath = QDir(templateDir).filePath(".universal_agent.version");
        const QString desiredVersion = QStringLiteral("3");

        if (!QDir().mkpath(templateDir)) {
            qWarning() << "[MainWindow] Could not create template directory:" << templateDir;
            return;
        }

        bool needsRewrite = true;
        {
            QFile versionFile(versionFilePath);
            if (versionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QString currentVersion = QString::fromUtf8(versionFile.readAll()).trimmed();
                versionFile.close();
                if (currentVersion == desiredVersion)
                    needsRewrite = false;
            }
        }
        if (!needsRewrite)
            return;

        const auto writeSequence = [&](const QString& fileName, const PromptSequence& seq) -> bool {
            const QString path = QDir(templateDir).filePath(fileName);
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                qWarning() << "[MainWindow] Could not write universal template sequence file:" << path;
                return false;
            }
            file.write(QJsonDocument(seq.toJson()).toJson(QJsonDocument::Indented));
            file.close();
            return true;
        };

        const QStringList allCommands = {
            "ASK_USER", "OPEN_FILE", "OPEN_FILE_REGION",
            "SEARCH_PROJECT", "SEARCH_PROJECT_CASE_INSENSITIVE", "SEARCH_WEB",
            "LIST_FILES_IN_FOLDER", "CONTEXT_SHOW", "CONTEXT_CLEAR",
            "CREATE_FILE", "REPLACE_IN_FILE", "EDIT_IN_RANGE", "INSERT_AT_LINE", "APPEND_TO_FILE",
            "RENAME_FILE", "COPY_FILE", "DELETE_FILE",
            "CREATE_DIR", "DELETE_DIR", "APPLY_PATCH",
            "RUN_SHELL", "RUN_PYTHON",
            "GIT_STATUS", "GIT_DIFF", "GIT_RESTORE_FILE", "UNDO_LAST_CHANGE",
            "WRITE_DESCRIPTION", "OPEN_SEQUENCE", "CALL_SEQUENCE", "REASONING",
            "SHOW_RESULT", "OPEN_APP", "OPEN_URL", "CLIPBOARD_SET", "CLIPBOARD_GET"
        };

        PromptSequence mainSeq(QStringLiteral("Universal Agent Main"));
        mainSeq.setDescription(QStringLiteral("Universal multi-stage flow: analysis -> context -> implementation -> verification."));
        {
            PromptStep s1;
            s1.setTemplate(
                "Current date: {{CURRENT_DATE}}\n"
                "Current datetime: {{CURRENT_DATETIME}}\n\n"
                "User request:\n{{USER_QUERY}}\n\n"
                "Context:\n{{HISTORY}}\n{{CONTEXT_CHAIN}}\n\n"
                "First, produce a short plan and identify missing context.\n"
                "If context is missing, call SEARCH_PROJECT/OPEN_FILE_REGION/SEARCH_WEB.\n"
                "If this is mostly informational/search task, produce SHOW_RESULT after collecting enough evidence.\n"
                "If full implementation is needed, use OPEN_SEQUENCE with sequenceName from {{OPEN_SEQUENCE_INFO}}.\n"
                "Do not repeat the same SEARCH_WEB query in a loop.\n"
                "Return JSON commands only.\n\n"
                "Allowed commands:\n{{SUPPORTED_COMMANDS}}"
            );
            s1.setAllowedCommands(allCommands);
            mainSeq.addStep(s1);
        }
        {
            PromptStep s2;
            s2.setTemplate(
                "Build an execution plan based on gathered context.\n"
                "Project structure:\n{{PROJECT_STRUCTURE}}\n\n"
                "Open files:\n{{OPEN_FILES}}\n\n"
                "If the user request is already answerable, emit SHOW_RESULT now.\n"
                "Otherwise decide the next focused sequence using OPEN_SEQUENCE when useful (analysis/coding/testing).\n"
                "Never end this step with only repeated search commands.\n"
                "Return JSON commands only."
            );
            s2.setAllowedCommands(allCommands);
            mainSeq.addStep(s2);
        }

        PromptSequence fileAnalysisSeq(QStringLiteral("Universal Agent File Analysis"));
        fileAnalysisSeq.setDescription(QStringLiteral("Deep file-level and architecture analysis for the task."));
        {
            PromptStep s;
            s.setTemplate(
                "Analyze project files deeply for this request:\n{{USER_QUERY}}\n\n"
                "Use FILE_LIST, PROJECT_STRUCTURE, OPEN_FILE_REGION and SEARCH_PROJECT to find exact implementation points.\n"
                "Summarize hotspots and then OPEN_SEQUENCE to context-chain or code-generation.\n"
                "If this request is informational and no code edits are needed, emit SHOW_RESULT.\n"
                "Return JSON commands only.\n\n"
                "{{FILE_LIST}}\n{{PROJECT_STRUCTURE}}\n{{OPEN_SEQUENCE_INFO}}"
            );
            s.setAllowedCommands(allCommands);
            fileAnalysisSeq.addStep(s);
        }

        PromptSequence contextChainSeq(QStringLiteral("Universal Agent Context Chain"));
        contextChainSeq.setDescription(QStringLiteral("Use executed command context to guide the next actions."));
        {
            PromptStep s;
            s.setTemplate(
                "Analyze context chain and previous results:\n{{CONTEXT_CHAIN}}\n\n"
                "Detect outdated assumptions, missing checks, and next best action.\n"
                "If enough evidence is collected and no more actions are needed, emit SHOW_RESULT.\n"
                "If code changes are ready, OPEN_SEQUENCE to code-generation.\n"
                "If verification is needed, OPEN_SEQUENCE to testing.\n"
                "Return JSON commands only."
            );
            s.setAllowedCommands(allCommands);
            contextChainSeq.addStep(s);
        }

        PromptSequence codeGenSeq(QStringLiteral("Universal Agent Code Generation"));
        codeGenSeq.setDescription(QStringLiteral("Generate and refine code changes with minimal risk."));
        {
            PromptStep s1;
            s1.setTemplate(
                "Implement the requested change.\n"
                "Request: {{USER_QUERY}}\n"
                "Context files:\n{{OPEN_FILES}}\n\n"
                "Prefer minimal diffs, preserve architecture, and avoid unrelated edits.\n"
                "Create/edit files and run local checks as needed.\n"
                "Return JSON commands only."
            );
            s1.setAllowedCommands(allCommands);
            codeGenSeq.addStep(s1);
        }
        {
            PromptStep s2;
            s2.setTemplate(
                "Review your produced changes for regressions and obvious defects.\n"
                "Use GIT_DIFF/GIT_STATUS and targeted checks.\n"
                "Then OPEN_SEQUENCE to testing if validation is needed.\n"
                "If validation is already sufficient, emit SHOW_RESULT.\n"
                "Return JSON commands only."
            );
            s2.setAllowedCommands(allCommands);
            codeGenSeq.addStep(s2);
        }

        PromptSequence testingSeq(QStringLiteral("Universal Agent Testing"));
        testingSeq.setDescription(QStringLiteral("Validate implementation and provide final status."));
        {
            PromptStep s1;
            s1.setTemplate(
                "Run verification for the implemented change.\n"
                "Use available test/build/lint commands and summarize failures.\n"
                "If failures require fixes, OPEN_SEQUENCE to code-generation.\n"
                "If no runnable tests exist, still produce a concise SHOW_RESULT with manual verification notes.\n"
                "Return JSON commands only."
            );
            s1.setAllowedCommands(allCommands);
            testingSeq.addStep(s1);
        }
        {
            PromptStep s2;
            s2.setTemplate(
                "Prepare final concise result for the user.\n"
                "Include what changed, how validated, and residual risks.\n"
                "Use SHOW_RESULT for final message (mandatory).\n"
                "Return JSON commands only."
            );
            s2.setAllowedCommands(allCommands);
            testingSeq.addStep(s2);
        }

        if (!writeSequence(QStringLiteral("main.seq.json"), mainSeq) ||
            !writeSequence(QStringLiteral("file-analysis.seq.json"), fileAnalysisSeq) ||
            !writeSequence(QStringLiteral("context-chain.seq.json"), contextChainSeq) ||
            !writeSequence(QStringLiteral("code-generation.seq.json"), codeGenSeq) ||
            !writeSequence(QStringLiteral("testing.seq.json"), testingSeq)) {
            return;
        }

        SolutionTemplate tmpl(QStringLiteral("Universal Agent"));
        tmpl.setDescription(QStringLiteral("Universal multi-sequence agent for coding, search, editing and verification."));
        tmpl.setMainSequencePath(QStringLiteral("main.seq.json"));
        tmpl.addSequence(QStringLiteral("main.seq.json"));
        tmpl.addSequence(QStringLiteral("file-analysis.seq.json"));
        tmpl.addSequence(QStringLiteral("context-chain.seq.json"));
        tmpl.addSequence(QStringLiteral("code-generation.seq.json"));
        tmpl.addSequence(QStringLiteral("testing.seq.json"));

        tmpl.setTargetsForSequence(QStringLiteral("main"),
                                   QJsonArray{QStringLiteral("file-analysis"), QStringLiteral("context-chain"),
                                              QStringLiteral("code-generation"), QStringLiteral("testing")});
        tmpl.setTargetsForSequence(QStringLiteral("file-analysis"),
                                   QJsonArray{QStringLiteral("context-chain"), QStringLiteral("code-generation")});
        tmpl.setTargetsForSequence(QStringLiteral("context-chain"),
                                   QJsonArray{QStringLiteral("code-generation"), QStringLiteral("testing"), QStringLiteral("main")});
        tmpl.setTargetsForSequence(QStringLiteral("code-generation"),
                                   QJsonArray{QStringLiteral("testing"), QStringLiteral("main")});
        tmpl.setTargetsForSequence(QStringLiteral("testing"),
                                   QJsonArray{QStringLiteral("code-generation"), QStringLiteral("main")});

        if (!m_templateMgr.save(tmpl, templateDir)) {
            qWarning() << "[MainWindow] Could not save universal template metadata:" << templateDir;
            return;
        }

        QFile versionFile(versionFilePath);
        if (!versionFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "[MainWindow] Could not write universal template version file:" << versionFilePath;
        } else {
            versionFile.write(desiredVersion.toUtf8());
            versionFile.close();
        }

        qDebug() << "[MainWindow] UniversalAgent template upgraded to version" << desiredVersion;
    };
    ensureUniversalAgentTemplate();
    m_templateMgr.refresh();

    // Update templates in ChatDialog after templates have been loaded
    m_chat->setTemplateManager(&m_templateMgr);

    // Initialize file system watcher to monitor external file changes
    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onFileChanged);

    // Initialize the new ContextManager
    m_contextManager = new ContextManager(this);
    m_contextManager->setDevAssManager(&m_dev_manager);
    m_contextManager->setMainWindow(this);
    connect(m_contextManager, &ContextManager::contextChanged, this, [this]() {
        // When context changes, update the GUI display
        updateContextStatusDisplay();
    });
    updateContextStatusDisplay();

    connect(m_chat, &ChatDialog::signal_updatelastUserMessage, this, &MainWindow::slot_updatelastUserMessage);


    connect(ide_widget, &IDEWidget::signal_runSequenceDialog, this, &MainWindow::runSequenceDialog);

    connect(ide_widget, &IDEWidget::signal_updatelastUserMessage,this, &MainWindow::slot_updatelastUserMessage);

    connect(this, &MainWindow::missingFilesUpdated,
                ide_widget, &IDEWidget::updateMissingFiles);


    reOpenRootDir();

    if (m_centerSplitter)
        m_centerSplitter->loadState();
    mainSplitter->loadState();

    openSequenceGraph();
    const QList<int> storedSizes = mainSplitter->sizes();
    if (storedSizes.size() >= 3 && storedSizes.at(2) == 0) {
        QList<int> fallbackSizes = storedSizes;
        fallbackSizes[0] = qMax(200, fallbackSizes.at(0));
        fallbackSizes[1] = qMax(400, fallbackSizes.at(1));
        fallbackSizes[2] = 250;
        mainSplitter->setSizes(fallbackSizes);
    }

    // Initialize task status controller (timer + status bar)
    m_statusController->initStatusBar(statusBar());
    m_statusController->setModel(&m_model);

    //rescan();

    // ------------ Управление последовательностями ----------------
    // … (тот же код, что был ранее)
}


void MainWindow::openGraph()
{
    m_graphWindow->show();
    m_graphWindow->raise();
    m_graphWindow->activateWindow();
}

void MainWindow::openLightRag()
{
    if (!m_lightRagWidget) {
        m_lightRagWidget = new LightRagWidget();
        m_lightRagWidget->setAttribute(Qt::WA_DeleteOnClose, true);
    }

    m_lightRagWidget->show();
    m_lightRagWidget->raise();
    m_lightRagWidget->activateWindow();
}

void MainWindow::openSequenceGraph()
{
    m_sequenceEditor->openSequenceGraph();
}

void MainWindow::onChatTemplateSelectionChanged(const QString &templatePath)
{
    m_sequenceEditor->onChatTemplateSelectionChanged(templatePath);
}

void MainWindow::updateSequenceGraphHighlight()
{
    m_sequenceEditor->updateSequenceGraphHighlight();
}

void MainWindow::updateSequenceGraphTemplateSelection()
{
    m_sequenceEditor->updateSequenceGraphTemplateSelection();
}

void MainWindow::openSequenceEditorForName(const QString &sequenceName)
{
    m_sequenceEditor->openSequenceEditorForName(sequenceName);
}

void MainWindow::browse(){
    QString d = QFileDialog::getExistingDirectory(this,"Select project folder", m_projectRoot);
    if (d.isEmpty()) return;

    m_projectRoot = d;
    reOpenRootDir();
}

void MainWindow::rescan(){
    /* 0. Подгружаем .dapro перед сканированием */
    m_dev_manager.load();

    /* Путь к каталогу DevelopersAssistant – будет использоваться для фильтрации */
    const QString devAssPath = m_dev_manager.devAssDir().absolutePath();

    /* Корневой каталог проекта – просто строка, уже абсолютная */
    const QString projectRootPath = m_projectRoot;   // m_projectRoot – QString

    /* Пересканируем индекс проекта */
    m_index.setRoot(m_projectRoot);
    m_files->clear();

    QSet<QString> needRed_set;
    /* Идем по списку файлов проекта */
    for (const QString& rel : m_index.files()) {
        /* Полный абсолютный путь к файлу */
        const QString absPath = QDir(projectRootPath).filePath(rel);

        /* ---  Фильтр: игнорируем файлы из DevelopersAssistant  --- */
        if (absPath.startsWith(devAssPath))
            continue;            // пропускаем файл, как будто его нет

        /* Создаём элемент списка */
        QListWidgetItem* item = new QListWidgetItem(QDir::fromNativeSeparators(rel), m_files);

        /* Получаем данные из .dapro */
        FileDescription* fd = m_dev_manager.findFileDescription(absPath);
        QDateTime fileMod = QFileInfo(absPath).lastModified();

        /* Отметка цвета: красный – файл не описан / устарел, зелёный – актуален */
        bool needRed = false;
        if (!fd) {
            needRed = true;
        } else if (fileMod > fd->lastModTime) {
            needRed = true;
        }

        if (needRed){
            needRed_set.insert(absPath);
            item->setBackgroundColor(Qt::red);
        }
        else
            item->setBackgroundColor(Qt::green);   // можно оставить default

        /* Храним абсолютный путь в UserRole */
        item->setData(Qt::UserRole, absPath);
    }

    emit missingFilesUpdated(needRed_set);



    log("Rescanned project.");
}

void MainWindow::openSettings(){
    if (!m_llmSettingsWindow)
        return;

    if (m_llmSettingsWindow->isVisible()) {
        m_llmSettingsWindow->hide();
        settingsBtn->setText("LLM Settings");
        return;
    }

    m_llmSettingsWindow->reloadFromConfig();
    m_llmSettingsWindow->show();
    settingsBtn->setText("Hide LLM Settings");
}

void MainWindow::editBasePrompt(){
    PromptEditorDialog d(&m_promptMgr, this);
    d.exec();
}

// MainWindow.cpp
// ------------------------------------------

QString MainWindow::buildProjectContext() const
{
    const QString projectRootPath = m_projectRoot;   // QString
    const QString devAssPath = m_dev_manager.devAssDir().absolutePath();

    QString s = "Корневая папка: " + projectRootPath + "\nФайлы:\n";

    int file_counter = 0;
    for (const QString& f : m_index.files()) {
        const QString abs = QDir(projectRootPath).filePath(f);

        // --- пропускаем файлы из DevelopersAssistant и .dadescr
        if (abs.startsWith(devAssPath) ||
            f.startsWith(QStringLiteral("DevelopersAssistant") + QDir::separator()) ||
            f.endsWith(QStringLiteral(".dadescr")))
            continue;

        s += " - " + QDir::toNativeSeparators(abs) + '\n';
        ++file_counter;

        // ---------- 1. Считаем описание (если надо) ----------
        // В этом примере описания оставляем как есть – но можно удалить
        // или закомментировать блок ниже, если они вам не нужны.

        const QString rel = QDir(projectRootPath).relativeFilePath(abs);
        const QString descrPath =
            m_dev_manager.devAssDir().filePath(rel + ".dadescr");
        QFile descrFile(descrPath);
        if (descrFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString descr = QString::fromUtf8(descrFile.readAll());
            descrFile.close();

            if (!descr.isEmpty()) {
                s += "   Описание:\n";
                s += "   " + descr.replace("\n", "\n   ") + "\n";
            }
        }

        // ---------- 2. ЗАГРУЖАЕМ СОДЕРЖИМОЕ файла ----------
        /*QFile fContent(abs);
        if (fContent.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(fContent.readAll());
            fContent.close();

            if (!content.isEmpty()) {
                s += "   Содержимое:\n";
                s += "   " + content.replace("\n", "\n   ") + "\n";
            }
        }*/
    }

    if(file_counter == 0){
        s += "Файлов в проекте пока нет." + '\n';
    }

    // ---------- Контент файлов, открытых командой OPEN_FILE ----------
    if (!m_contextCache.contextContent().isEmpty())
        s += "\nСодержимое файлов, добавленных в контекст:\n" + m_contextCache.contextContent() + "\n";

    // ---------- Context chain from command execution ----------
    if (m_contextManager) {
        QString contextChain = m_contextManager->generateContextText();
        if (!contextChain.isEmpty()) {
            s += "\nЦепочка контекста из выполненных команд:\n" + contextChain + "\n";
        }
    }

    return s;
}

void MainWindow::applyChanges(){
    if (m_pending.isEmpty()) { QMessageBox::information(this,"Nothing to apply","No pending changes."); return; }
    ChangesDialog dlg(this);
    dlg.setChanges(m_pending);
    if (dlg.exec()==QDialog::Accepted) {
        QString err;
        QList<Change> copy = m_pending;
        if (!m_applier.apply(copy, &err)) { QMessageBox::critical(this,"Apply failed", err); return; }
        log(QString("Applied %1 change(s). ").arg(m_pending.size()));
        m_pending.clear();
        rescan();
    } else {
        log("Changes canceled.");
    }
}

void MainWindow::undoLast(){
    QString err; if (!m_applier.undo(&err)) { QMessageBox::warning(this,"Undo", err.isEmpty()?"Nothing to undo":err); }
    else { log("Undo done."); rescan(); }
}

void MainWindow::sendUserMessage(const QString& text){
    if (m_contextManager)
        m_contextManager->observeUserQuery(text);

    m_lastUserMessage = text;
    resetSequenceStabilityState();
    qDebug() << "[sendUserMessage] updated last user message =" << m_lastUserMessage;
    appendUserQuestionToResultDisplay(text);

    // Add to main history
    m_history.push_back(qMakePair(QString("user"), text));
    // Also add to separate user messages storage (for LAST_USER_MESSAGES resolver)
    m_lastUserMessages.append(text);
    // Keep only last N messages
    while (m_lastUserMessages.size() > MAX_USER_MESSAGES) {
        m_lastUserMessages.removeFirst();
    }
    // Debug info about LAST_USER_MESSAGES storage
    qDebug() << "[sendUserMessage] Added message to LAST_USER_MESSAGES storage. Current count:" << m_lastUserMessages.size();
    qDebug() << "[sendUserMessage] Last message in storage:" << (m_lastUserMessages.isEmpty() ? QString("empty") : m_lastUserMessages.last());

    // Debug info about managers before running sequence
    qDebug() << "[sendUserMessage] About to call runSequenceDialog";
    qDebug() << "[sendUserMessage] Template manager root:" << m_templateMgr.root();
    qDebug() << "[sendUserMessage] Sequence manager root:" << m_seqMgr.root();

    runSequenceDialog();

    qDebug() << "[sendUserMessage] runSequenceDialog completed";

}

void MainWindow::onModelChunk(const QString& s){
    m_streamBuf += s;
    if (m_chat) m_chat->appendAssistantChunk(s);
}

QByteArray MainWindow::extractJsonPart(const QByteArray& txt) const
{
    QList<QByteArray> parts = splitConcatenatedJson_fromText(QString::fromUtf8(txt));
    if (parts.isEmpty())
        return QByteArray();

    if (parts.size() == 1) {
        const QByteArray trimmed = parts.first().trimmed();
        if (!trimmed.isEmpty() && trimmed.startsWith('['))
            return trimmed;
    }

    QByteArray joined = "[";
    for (int i = 0; i < parts.size(); ++i) {
        if (i)
            joined += ',';
        joined += parts.at(i);
    }
    joined += "]";
    return joined;
}

void MainWindow::editSequences()
{
    m_sequenceEditor->editSequences();
}

void MainWindow::runSequenceDialog()
{
    m_sequenceRunner->runSequenceDialog();
}

void MainWindow::runNextStep()
{
    m_sequenceRunner->runNextStep();
}
void MainWindow::handleAssistantFinished()
{
    m_sequenceRunner->handleAssistantFinished();
}

void MainWindow::handleOpenCommand(Command* cmd)
{
    const QJsonObject params = cmd->params();
    const QString rawPath = params.value("path").toString();
    if (rawPath.isEmpty())
        return;

    const QString normalized = normalizeContextPath(rawPath, m_projectRoot);
    QFile file(normalized);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    const QString text = QString::fromUtf8(file.readAll());
    file.close();

    QStringList allLines = text.split('\n', QString::KeepEmptyParts);
    if (text.endsWith('\n') && !allLines.isEmpty())
        allLines.removeLast();
    if (cmd->name() == "OPEN_FILE") {
        m_contextCache.setFullFileContext(normalized, allLines);
        // Track that this file's content is in context from OPEN_FILE command
        m_fileToContextCommands[normalized].insert("OPEN_FILE");
        // Add file to watcher to monitor for external changes
        if (!m_fileWatcher->files().contains(normalized) && QFileInfo::exists(normalized)) {
            m_fileWatcher->addPath(normalized);
        }
        // Add command to context manager
        if (m_contextManager) {
            QString result = QString("OPEN_FILE: %1 (full content)").arg(rawPath);
            addCommandToContext(cmd, result);
        }
        return;
    }

    int startLine = params.value("startLine").toInt(-1);
    int endLine = params.value("endLine").toInt(-1);
    QStringList snippet;

    if (startLine >= 1 && endLine >= startLine) {
        // Validate that the requested line numbers don't exceed the total number of lines in the file
        if (startLine > allLines.size()) {
            // Error: requested start line is beyond the end of the file
            QString errorMsg = QString("ERROR: Cannot OPEN_FILE_REGION %1: requested start line %2 is beyond file length of %3 lines. Remember: line numbering starts from 1, not 0.")
                    .arg(rawPath).arg(startLine).arg(allLines.size());
            log(errorMsg);
            if (m_contextManager) {
                addCommandToContext(cmd, errorMsg);
            }
            return;
        }

        if (endLine > allLines.size()) {
            // Error: requested end line is beyond the end of the file
            QString errorMsg = QString("ERROR: Cannot OPEN_FILE_REGION %1: requested end line %2 is beyond file length of %3 lines. Remember: line numbering starts from 1, not 0.")
                    .arg(rawPath).arg(endLine).arg(allLines.size());
            log(errorMsg);
            if (m_contextManager) {
                addCommandToContext(cmd, errorMsg);
            }
            return;
        }

        startLine = qMax(1, startLine);
        endLine = qMin(endLine, allLines.size());
        snippet = allLines.mid(startLine - 1, endLine - startLine + 1);
    } else {
        const QString region = params.value("region").toString();
        if (region.isEmpty())
            return;
        QRegularExpression re(region, QRegularExpression::DotMatchesEverythingOption
                                        | QRegularExpression::MultilineOption);
        auto match = re.match(text);
        if (!match.hasMatch())
            return;
        const int startIndex = match.capturedStart();
        startLine = text.left(startIndex).count('\n') + 1;
        const QString captured = match.captured(0);
        snippet = captured.split('\n', QString::KeepEmptyParts);
        if (captured.endsWith('\n') && !snippet.isEmpty())
            snippet.removeLast();
    }

    if (!snippet.isEmpty()) {
        m_contextCache.addRegionContext(normalized, startLine, snippet);
        // Track that this file's content is in context from OPEN_FILE_REGION command
        m_fileToContextCommands[normalized].insert("OPEN_FILE_REGION");
        // Add file to watcher to monitor for external changes
        if (!m_fileWatcher->files().contains(normalized) && QFileInfo::exists(normalized)) {
            m_fileWatcher->addPath(normalized);
        }
        // Add command to context manager - include the actual numbered content
        if (m_contextManager) {
            QString numberedContent = m_contextCache.formatNumberedLines(snippet, startLine);
            QString result = QString("OPEN_FILE_REGION: %1\n%2").arg(rawPath, numberedContent);
            addCommandToContext(cmd, result);
        }
    }
}


bool MainWindow::applyGroupedEditInRange(const QList<Command*>& commands, QString* err)
{
    QMap<QString, QList<EditInRangeCommand*>> grouped;
    for (Command* c : commands) {
        auto edit = dynamic_cast<EditInRangeCommand*>(c);
        if (!edit)
            continue;
        const QString normalized = normalizeContextPath(edit->path(), m_projectRoot);
        grouped[normalized].append(edit);
    }

    for (auto it = grouped.begin(); it != grouped.end(); ++it) {
        if (!EditInRangeCommand::applyBatch(it.value(), &m_dev_manager, err))
            return false;
        m_contextCache.clearContextForPath(it.key());
    }
    return true;
}

void MainWindow::handlePostApplyContextCleanup(Command* cmd)
{
    const QString name = cmd->name();
    const QJsonObject p = cmd->params();
    auto clear = [&](const QString& key) {
        if (!key.isEmpty())
            m_contextCache.clearContextForPath(key);
    };

    if (name == "EDIT_IN_RANGE" || name == "REPLACE_IN_FILE"
        || name == "DELETE_FILE" || name == "CREATE_FILE"
        || name == "APPEND_TO_FILE" || name == "INSERT_AT_LINE") {
        clear(p.value("path").toString());
        return;
    }

    if (name == "RENAME_FILE") {
        clear(p.value("oldPath").toString());
        clear(p.value("newPath").toString());
        return;
    }

    if (name == "COPY_FILE") {
        clear(p.value("to").toString());
        return;
    }

    if (name == "GIT_RESTORE_FILE") {
        clear(p.value("path").toString());
        return;
    }

    if (name == "APPLY_PATCH") {
        m_contextCache.clearAll();
        return;
    }

    if (name == "UNDO_LAST_CHANGE") {
        // We don't know which files changed; clear all file-context slices.
        m_contextCache.clearAll();
        return;
    }

    // Handle RunShellCommand - remove old output when command is run again
    if (name == "RUN_SHELL") {
        QString command = p.value("command").toString();
        if (!command.isEmpty()) {
            m_contextCache.removeConsoleLogsWithPrefix("==== RUN_SHELL: " + command);
            m_shellCommandOutputs.remove(command);
        }
        return;
    }

    // Handle RunPythonCommand - remove old output when same script is run again
    if (name == "RUN_PYTHON") {
        QString scriptPath = p.value("scriptPath").toString();
        QString code = p.value("code").toString();
        QString identifier = !scriptPath.isEmpty() ? scriptPath : "inline code";

        if (!identifier.isEmpty()) {
            m_contextCache.removeConsoleLogsWithPrefix("==== RUN_PYTHON: " + identifier);
            m_pythonScriptOutputs.remove(identifier);
        }
        return;
    }

    // Handle OpenFileContextCommand and OpenFileRegionCommand - add to file watcher
    if (name == "OPEN_FILE" || name == "OPEN_FILE_REGION") {
        QString path = p.value("path").toString();
        if (!path.isEmpty()) {
            const QString normalized = normalizeContextPath(path, m_projectRoot);
            // Add file to watcher to monitor for external changes
            if (!m_fileWatcher->files().contains(normalized) && QFileInfo::exists(normalized)) {
                m_fileWatcher->addPath(normalized);
            }
        }
    }
}

void MainWindow::handleSearchResults(Command* cmd)
{
    if (auto web = dynamic_cast<SearchWebCommand*>(cmd)) {
        if (web->results().isEmpty())
            return;
        QString query = cmd->params().value("query").toString();
        QString title = QString("==== SEARCH_WEB: %1 ====").arg(query);
        QString body = web->results().join("\n\n");
        qDebug() << "[MainWindow] SEARCH_WEB results count:" << web->results().size();
        qDebug() << "[MainWindow] SEARCH_WEB result body length:" << body.length();
        if (!body.isEmpty()) {
            qDebug() << "[MainWindow] SEARCH_WEB first 200 chars of result:" << body.left(200);
        } else {
            qDebug() << "[MainWindow] SEARCH_WEB result body is empty";
        }

        // Проверяем, содержит ли результат пометку о необходимости суммаризации
        bool needsSummarization = body.contains("Контент для суммаризации", Qt::CaseInsensitive);

        QString resultBody = body;
        if (needsSummarization) {
            // Помечаем, что результат требует суммаризации (в реальной реализации вызовем LLM асинхронно)
            resultBody = summarizeSearchResults(body, query);
        }

        appendExternalContext(QString("%1\n%2").arg(title, resultBody));

        // Add to context manager
        if (m_contextManager) {
            qDebug() << "[MainWindow] Adding SEARCH_WEB command to context manager";
            addCommandToContext(cmd, QString("%1\n%2").arg(title, resultBody));
        } else {
            qDebug() << "[MainWindow] Context manager is null, skipping context addition";
        }
        return;
    }
    if (auto local = dynamic_cast<SearchProjectCommand*>(cmd)) {
        if (local->matches().isEmpty())
            return;
        QString title = QString("==== SEARCH_PROJECT: %1 ====").arg(cmd->params().value("query").toString());
        QString body = local->matches().join("\n\n");
        appendExternalContext(QString("%1\n%2").arg(title, body));

        // Add to context manager
        if (m_contextManager) {
            addCommandToContext(cmd, QString("%1\n%2").arg(title, body));
        }
    }
}

QString MainWindow::searchResultsText() const
{
    return m_contextCache.externalSources().join("\n\n");
}

void MainWindow::appendConsoleContext(const QString& title,
                                      const QString& stdOut,
                                      const QString& stdErr,
                                      int exitCode)
{
    QString block = QString("==== %1 ====\n").arg(title.isEmpty() ? "Console" : title);
    if (!stdOut.trimmed().isEmpty()) {
        block += "[stdout]\n" + stdOut.trimmed() + "\n";
    }
    if (!stdErr.trimmed().isEmpty()) {
        block += "[stderr]\n" + stdErr.trimmed() + "\n";
    }
    block += QString("exitCode=%1").arg(exitCode);

    // If this is a RUN_SHELL command output, replace existing output for the same command
    if (title.startsWith("RUN_SHELL: ")) {
        QString cmd = title.mid(11); // Extract the command part after "RUN_SHELL: "
        m_contextCache.removeConsoleLogsWithPrefix("==== RUN_SHELL: " + cmd);
        m_shellCommandOutputs.remove(cmd);
    }
    // If this is a RUN_PYTHON command output, replace existing output for the same script
    else if (title.startsWith("RUN_PYTHON: ")) {
        QString script = title.mid(12); // Extract the script part after "RUN_PYTHON: "
        m_contextCache.removeConsoleLogsWithPrefix("==== RUN_PYTHON: " + script);
        m_pythonScriptOutputs.remove(script);
    }

    m_contextCache.appendConsoleLog(block.trimmed());
    // Limit to 5 logs
    while (m_contextCache.consoleLogs().size() > 5) {
        auto logs = m_contextCache.consoleLogs();
        m_contextCache.clearConsoleLogs();
        for (int i = 0; i < qMin(5, logs.size()); ++i) {
            m_contextCache.appendConsoleLog(logs.at(i));
        }
    }
}

void MainWindow::appendExternalContext(const QString& block)
{
    if (block.trimmed().isEmpty())
        return;
    m_contextCache.appendExternalSource(block.trimmed());
    // Limit to 5 blocks
    while (m_contextCache.externalSources().size() > 5) {
        // Workaround: clear and re-add all but first 5
        auto sources = m_contextCache.externalSources();
        m_contextCache.clearExternalSources();
        for (int i = 0; i < qMin(5, sources.size()); ++i) {
            m_contextCache.appendExternalSource(sources.at(i));
        }
    }
}

void MainWindow::resetSequenceStabilityState()
{
    m_sequenceRunner->resetSequenceStabilityState();
}

QString MainWindow::commandBatchSignature(const QList<Command*>& commands) const
{
    return m_sequenceRunner->commandBatchSignature(commands);
}

bool MainWindow::isReadOnlyCommandBatch(const QList<Command*>& commands) const
{
    return m_sequenceRunner->isReadOnlyCommandBatch(commands);
}

void MainWindow::appendResultToUi(const QString& title, const QString& content)
{
    m_sequenceRunner->appendResultToUi(title, content);
}

void MainWindow::autoFinalizeSequenceWithResult(const QString& reason, const QList<Command*>& recentCommands)
{
    m_sequenceRunner->autoFinalizeSequenceWithResult(reason, recentCommands);
}

void MainWindow::retryCurrentSequenceStep()
{
    m_sequenceRunner->retryCurrentSequenceStep();
}
bool MainWindow::ensureAgentModelConfigured()
{
    return m_sequenceRunner->ensureAgentModelConfigured();
}

QStringList MainWindow::buildAllowedCommandsDocsCompact(const PromptStep& step) const
{
    return m_sequenceRunner->buildAllowedCommandsDocsCompact(step);
}

QByteArray MainWindow::repairToJsonArrayIfNeeded(const QByteArray& rawText, const PromptStep& step)
{
    return m_sequenceRunner->repairToJsonArrayIfNeeded(rawText, step);
}

bool MainWindow::isImplementationIntentText(const QString& text)
{
    return SequenceRunner::isImplementationIntentText(text);
}

bool MainWindow::didAnyAffectedPathChangeSince(const QDateTime& since, const QList<Command*>& executedCommands) const
{
    return m_sequenceRunner->didAnyAffectedPathChangeSince(since, executedCommands);
}

bool MainWindow::maybeAutoRepairAfterExecution(const PromptStep& step,
                                              const QDateTime& execStartedAt,
                                              const QList<Command*>& executedCommands,
                                              bool applyFailed,
                                              const QString& lastError)
{
    return m_sequenceRunner->maybeAutoRepairAfterExecution(step, execStartedAt, executedCommands, applyFailed, lastError);
}

void MainWindow::runNextSequence()
{
    m_sequenceRunner->runNextSequence();
}

void MainWindow::addSequenceTransitionToHistory(const QString& sequenceName)
{
    m_sequenceRunner->addSequenceTransitionToHistory(sequenceName);
}

void MainWindow::generateDescriptions()
{
    DescriptionGenerator generator(&m_model, &m_dev_manager, this);

    // 1. Перебираем файлы проекта
    QStringList fileList;
    QDirIterator it(m_projectRoot, QDir::Files, QDirIterator::Subdirectories);
    const QString devAssPath = m_dev_manager.devAssDir().absolutePath();

    while (it.hasNext()) {
        const QString srcFilePath = it.next();
        if (srcFilePath.startsWith(devAssPath))
            continue; // игнорируем DevAss

        FileDescription* fd = m_dev_manager.findFileDescription(srcFilePath);
        QDateTime fileMod = QFileInfo(srcFilePath).lastModified();

        if (fd && fd->isDescriptionUpToDate && fileMod <= fd->lastModTime)
            continue; // уже актуально

        fileList << srcFilePath;
    }

    if (fileList.isEmpty()) {
        QMessageBox::information(this, "Готово",
                                 "В проекте нет файлов, требующих описания.");
        return;
    }

    // 2. Генерация с прогресс‑диалогом
    QProgressDialog prog("Генерация описаний…", "Отмена",
                         0, fileList.size(), this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(0);

    int processed = 0, failed = 0;
    for (const QString &srcFilePath : fileList) {
        if (prog.wasCanceled()) break;

        QString descr = generator.generateAndSave(srcFilePath);
        if (descr.isEmpty()){
            ++failed;
        }
        else{
            // 1. Относительный путь к исходному файлу
            QString relPath = QDir(m_projectRoot).relativeFilePath(srcFilePath);

            // 2. Путь к файлу‑дескриптору *.dadescr
            QString descrPath = m_dev_manager.devAssDir().filePath(relPath + ".dadescr");

            // 3. Имя файла дескриптора
            QString descrFileName = QFileInfo(descrPath).fileName();

            // 4. Логируем
            log(QString("✓ Описан %1 → %2").arg(relPath).arg(descrFileName));
        }
        ++processed;
        prog.setValue(processed);
    }

    prog.setValue(fileList.size());
    log(QString("Генерация завершена: %1 успешно, %2 ошибок.")
        .arg(processed - failed).arg(failed));

    rescan();
}

void MainWindow::log(const QString& m){ m_log->append(m); }

// <‑‑ Добавлен новый слот для обработки текста от Vosk
void MainWindow::onVoskText(const QString& text){
    log("Vosk recognized: " + text);
    sendUserMessage(text);
}

void MainWindow::slot_updatelastUserMessage(const QString &lastUserMessage)
{
    if (lastUserMessage.trimmed().isEmpty())
        return;
    m_lastUserMessage = lastUserMessage;
    qDebug()<<"m_lastUserMessage = "<<m_lastUserMessage;
}

/* ────────────────────────────────────────────────────── */
void MainWindow::ensureDefaultSequence()
{
    m_sequenceEditor->ensureDefaultSequence();
}

void MainWindow::reOpenRootDir(){
    if(m_projectRoot.isEmpty()){
        return;
    }

    // Clear file watcher and context mappings when switching projects
    m_fileWatcher->removePaths(m_fileWatcher->files());
    m_fileToContextCommands.clear();
    m_shellCommandOutputs.clear();
    m_pythonScriptOutputs.clear();

    ide_widget->openProjectbyPath(m_projectRoot);

    m_rootEdit->setText(m_projectRoot);
    m_dev_manager.setProjectRoot(m_projectRoot);
    m_promptMgr.setProjectRoot(m_projectRoot);

    if (!m_dev_manager.createFolder() || !m_dev_manager.createDaproFile())
    {
        qCritical() << "Не удалось подготовить DevelopersAssistant / .dapro";
        //return -1;
    }

    if (!m_dev_manager.generateDescrFiles())
    {
        qCritical() << "Ошибка при создании .dadescr файлов";
    }

    rescan();
}

void MainWindow::setupModelWithGenerationParams()
{
    // Загружаем параметры генерации
    AppConfig cfg;
    cfg.load();

    // Подготовим карту параметров для Ollama
    QVariantMap genParams;
    genParams["temperature"] = cfg.temperature;
    genParams["top_p"] = cfg.topP;
    genParams["top_k"] = cfg.topK;
    genParams["repeat_penalty"] = cfg.repeatPenalty;
    genParams["presence_penalty"] = cfg.presencePenalty;
    genParams["frequency_penalty"] = cfg.frequencyPenalty;
    genParams["num_predict"] = cfg.numPredict;
    genParams["num_ctx"] = cfg.numCtx;
    genParams["mirostat"] = cfg.mirostat;
    genParams["mirostat_tau"] = cfg.mirostatTau;
    genParams["mirostat_eta"] = cfg.mirostatEta;
    genParams["seed"] = cfg.seed;

    // Если есть stop-слова, добавим их
    if (!cfg.stopWords.isEmpty()) {
        QJsonArray stopArray;
        for (const QString& word : cfg.stopWords) {
            stopArray.append(word);
        }
        genParams["stop"] = stopArray;
    }

    genParams["reasoning_effort"] = cfg.reasoningEffort;
    genParams["temperature_decay"] = cfg.temperatureDecay;
    genParams["repeat_last_n"] = cfg.repeatLastN;

    // Применяем настройки модели
    if (!m_model.selectedModel().isEmpty()) {
        m_model.configure(m_model.selectedModel(), genParams);
    }
}



QString MainWindow::previousStepsText() const
{
    if (m_prevSteps.isEmpty()) return QString("Пока ничего не выполнено.");
    QString out;
    for (const QString& s : m_prevSteps) out += " - " + s + "\n";
    return out.trimmed();
}

QString MainWindow::fileDescriptionsText() const
{
    QString out;
    QDir dev = m_dev_manager.devAssDir();
    if (!dev.exists()) return out;

    // Iterate through all *.dadescr files and include their content with absolute target path
    QDirIterator it(dev.absolutePath(), QStringList()<<"*.dadescr", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()){
        QString descrPath = it.next();
        QFile f(descrPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QString descr = QString::fromUtf8(f.readAll());
        f.close();

        // Derive original file path (without .dadescr) relative to project root
        QString rel = dev.relativeFilePath(descrPath);
        rel.chop(QString(".dadescr").size());
        QString absTarget = QDir(m_projectRoot).filePath(rel);

        out += "==== " + QDir::toNativeSeparators(absTarget) + " ====\n";
        out += descr.trimmed() + "\n\n";
    }
    return out.trimmed();
}

QString MainWindow::getContextChainText() const
{
    if (m_contextManager) {
        return m_contextManager->generateContextText();
    }
    return QString();
}

QString MainWindow::getAllCommandsText() const
{
    if (m_contextManager) {
        // Generate text from ALL commands, not just active ones
        QStringList contextParts;
        QList<CommandContextInfo> allCommands = m_contextManager->getAllCommands();

        for (const auto& cmdInfo : allCommands) {
            QString commandText = cmdInfo.result;
            if (commandText.isEmpty()) {
                // If no specific result, use description as fallback
                commandText = cmdInfo.description;
            }

            if (!commandText.isEmpty()) {
                QString status = "";
                switch (cmdInfo.state) {
                    case CommandState::Active:
                        status = " [ACTIVE]";
                        break;
                    case CommandState::Invalidated:
                        status = QString(" [INVALIDATED: %1]").arg(cmdInfo.reason);
                        break;
                    case CommandState::Removed:
                        status = " [REMOVED]";
                        break;
                }
                contextParts << commandText + status;
            }
        }
        return contextParts.join("\n\n");
    }
    return QString();
}

void MainWindow::createNewSequence()
{
    m_sequenceEditor->createNewSequence();
}

// Вспомогательная функция для суммаризации результатов поиска
QString MainWindow::summarizeSearchResults(const QString& content, const QString& query)
{
    const QString trimmedContent = content.trimmed();
    if (trimmedContent.isEmpty()) {
        return QStringLiteral("Результаты поиска не содержат текста для суммаризации.");
    }

    if (m_model.selectedModel().isEmpty()) {
        qWarning() << "[MainWindow] Summarization skipped: LLM model not configured";
        return QStringLiteral("LLM не настроен, суммаризация недоступна.");
    }

    QString effectiveQuery = query.trimmed();
    if (effectiveQuery.isEmpty())
        effectiveQuery = QStringLiteral("поисковый запрос без текста");

    QString contentForPrompt = trimmedContent;
    QString truncationNote;
    constexpr int kMaxContextChars = 12000;
    if (contentForPrompt.size() > kMaxContextChars) {
        contentForPrompt = contentForPrompt.left(kMaxContextChars);
        truncationNote = QStringLiteral("\n[Контент обрезан до %1 символов]").arg(kMaxContextChars);
    }

    const QString systemPrompt = QStringLiteral(
        "Ты ассистент-разработчика. Сформируй на основе найденного обзора краткую, структурированную суммаризацию. "
        "Укажи ключевые выводы, ссылки на источники и рекомендуемые действия. Отвечай на русском языке, не выдумывай фактов."
    );
    const QString userPrompt = QStringLiteral(
        "Запрос: %1\n"
        "Содержимое результатов:\n%2%3\n\n"
        "Выведи итоговое резюме (3–4 тезиса или коротких абзаца) с указанием ключевых фактов, ссылок и возможных действий."
    ).arg(effectiveQuery, contentForPrompt, truncationNote);

    qDebug() << "[MainWindow] Summarizing SEARCH_WEB results for query:" << effectiveQuery
             << "context length:" << contentForPrompt.size();

    QString summary = m_model.generateWithSystem(userPrompt, systemPrompt).trimmed();
    const QString errorPrefix = QStringLiteral("[Ollama error]");
    if (summary.startsWith(errorPrefix)) {
        qWarning() << "[MainWindow] LLM error during search summary:" << summary;
        summary = QStringLiteral("LLM вернул ошибку: %1").arg(summary.mid(errorPrefix.length()).trimmed());
    } else if (summary.isEmpty()) {
        qWarning() << "[MainWindow] LLM returned an empty summary for SEARCH_WEB";
        summary = QStringLiteral("LLM не вернула ответ на суммаризацию.");
    }

    return summary;
}


QString MainWindow::getLastUserMessages() const
{
    QString result = m_lastUserMessages.join("\n");
    qDebug() << "[getLastUserMessages] Returning" << m_lastUserMessages.size() << "messages, total length:" << result.length();
    if (m_lastUserMessages.size() > 0) {
        qDebug() << "[getLastUserMessages] First message:" << m_lastUserMessages.first();
        qDebug() << "[getLastUserMessages] Last message:" << m_lastUserMessages.last();
    }
    return result;
}

void MainWindow::addCommandToContext(Command* cmd, const QString& result)
{
    if (!m_contextManager || !cmd) {
        qDebug() << "[MainWindow] Context manager is null or cmd is null, skipping context addition";
        return;
    }

    qDebug() << "[MainWindow] Adding command" << cmd->name() << "to context with result length:" << result.length();

    // If we're in a sequence mode, add to the current sequence, otherwise add directly
    if (m_sequenceMode && !m_currentSeqId.isEmpty()) {
        qDebug() << "[MainWindow] Adding command to sequence" << m_currentSeqId;
        m_contextManager->addCommandToSequence(cmd, m_currentSeqId, result);
    } else {
        qDebug() << "[MainWindow] Adding command directly to context manager";
        // The ContextManager will handle duplicate detection internally
        m_contextManager->addCommand(cmd, result);
    }
}

void MainWindow::refreshAllSequences()
{
    m_sequenceEditor->refreshAllSequences();
}

void MainWindow::updateContextStatusDisplay()
{
    if (!m_contextCommandsList || !m_contextManager) {
        return;
    }

    m_contextCommandsList->clear();

    // Get all sequences from context manager
    QList<CommandSequenceInfo> allSequences = m_contextManager->getCommandSequences();

    if (allSequences.isEmpty()) {
        // If no sequences, fall back to showing individual commands as before
        QList<CommandContextInfo> allCommands = m_contextManager->getAllCommands();

        for (const auto& cmdInfo : allCommands) {
            // Create top-level item for the command
            QTreeWidgetItem* commandItem = new QTreeWidgetItem(m_contextCommandsList);

            // First column: timestamp
            commandItem->setText(0, cmdInfo.timestamp.toString("hh:mm:ss"));

            // Second column: description
            commandItem->setText(1, cmdInfo.description);

            // If this command belongs to a sequence, indicate that
            if (!cmdInfo.sequenceId.isEmpty()) {
                CommandSequenceInfo seqInfo = m_contextManager->getSequenceInfo(cmdInfo.sequenceId);
                commandItem->setText(1, QString("[%1] %2").arg(seqInfo.name).arg(cmdInfo.description));
            }

            switch (cmdInfo.state) {
                case CommandState::Active: {
                    // Active commands in context shown in green
                    commandItem->setBackground(0, QColor(200, 255, 200)); // Light green
                    commandItem->setBackground(1, QColor(200, 255, 200)); // Light green
                    commandItem->setForeground(0, Qt::darkGreen);
                    commandItem->setForeground(1, Qt::darkGreen);
                    commandItem->setToolTip(0, "Active in context");
                    commandItem->setToolTip(1, "Active in context");
                    break;
                }
                case CommandState::Invalidated: {
                    // Invalidated commands shown in red with reason
                    commandItem->setBackground(0, QColor(255, 200, 200)); // Light red
                    commandItem->setBackground(1, QColor(255, 200, 200)); // Light red
                    commandItem->setForeground(0, Qt::red);
                    commandItem->setForeground(1, Qt::red);
                    QString tooltip = QString("Invalidated: %1").arg(cmdInfo.reason);
                    commandItem->setToolTip(0, tooltip);
                    commandItem->setToolTip(1, tooltip);
                    break;
                }
                case CommandState::Removed: {
                    // Removed commands shown in gray
                    commandItem->setBackground(0, QColor(200, 200, 200)); // Light gray
                    commandItem->setBackground(1, QColor(200, 200, 200)); // Light gray
                    commandItem->setForeground(0, Qt::gray);
                    commandItem->setForeground(1, Qt::gray);
                    commandItem->setToolTip(0, "Command was removed from context");
                    commandItem->setToolTip(1, "Command was removed from context");
                    break;
                }
            }

            // Store command ID in userData for potential future use
            commandItem->setData(0, Qt::UserRole, cmdInfo.id);

            // Add result as child item if it exists
            if (!cmdInfo.result.isEmpty()) {
                QTreeWidgetItem* resultItem = new QTreeWidgetItem(commandItem);
                resultItem->setText(0, "Result:");
                resultItem->setText(1, cmdInfo.result);
                resultItem->setBackground(0, QColor(240, 240, 240)); // Light gray background for result header
                resultItem->setBackground(1, QColor(245, 245, 245)); // Light gray background
                resultItem->setForeground(0, Qt::darkBlue);
                resultItem->setForeground(1, Qt::darkBlue);

                // Format the result in a more readable way if it's long
                QStringList resultLines = cmdInfo.result.split('\n');
                if (resultLines.size() > 10) {
                    // Show first 10 lines and indicate more
                    QStringList shortResult = resultLines.mid(0, 10);
                    shortResult << "... (click to expand)";
                    resultItem->setText(1, shortResult.join('\n'));
                } else {
                    resultItem->setText(1, cmdInfo.result);
                }
            }
        }
    } else {
        // Show commands organized by sequences
        for (const auto& seqInfo : allSequences) {
            // Create top-level item for the sequence
            QTreeWidgetItem* seqItem = new QTreeWidgetItem(m_contextCommandsList);

            // First column: timestamp
            seqItem->setText(0, seqInfo.timestamp.toString("hh:mm:ss"));

            // Second column: sequence name with description
            seqItem->setText(1, QString("SEQUENCE: %1").arg(seqInfo.name));
            if (!seqInfo.description.isEmpty()) {
                seqItem->setToolTip(1, seqInfo.description);
            }

            // Set different color for sequence items (to distinguish from commands)
            seqItem->setBackground(0, QColor(255, 235, 205)); // Light orange background for sequences
            seqItem->setBackground(1, QColor(255, 235, 205)); // Light orange background
            seqItem->setForeground(0, Qt::darkBlue);
            seqItem->setForeground(1, Qt::darkBlue);

            // Store sequence ID in userData for potential future use
            seqItem->setData(0, Qt::UserRole, seqInfo.id);

            // Get commands in this sequence and add them as child items
            QList<CommandContextInfo> commandsInSequence = m_contextManager->getCommandsInSequence(seqInfo.id);
            for (const auto& cmdInfo : commandsInSequence) {
                QTreeWidgetItem* commandItem = new QTreeWidgetItem(seqItem);

                // First column: timestamp
                commandItem->setText(0, cmdInfo.timestamp.toString("hh:mm:ss"));

                // Second column: command description
                commandItem->setText(1, cmdInfo.description);

                switch (cmdInfo.state) {
                    case CommandState::Active: {
                        // Active commands in context shown in green
                        commandItem->setBackground(0, QColor(200, 255, 200)); // Light green
                        commandItem->setBackground(1, QColor(200, 255, 200)); // Light green
                        commandItem->setForeground(0, Qt::darkGreen);
                        commandItem->setForeground(1, Qt::darkGreen);
                        commandItem->setToolTip(0, "Active in context");
                        commandItem->setToolTip(1, "Active in context");
                        break;
                    }
                    case CommandState::Invalidated: {
                        // Invalidated commands shown in red with reason
                        commandItem->setBackground(0, QColor(255, 200, 200)); // Light red
                        commandItem->setBackground(1, QColor(255, 200, 200)); // Light red
                        commandItem->setForeground(0, Qt::red);
                        commandItem->setForeground(1, Qt::red);
                        QString tooltip = QString("Invalidated: %1").arg(cmdInfo.reason);
                        commandItem->setToolTip(0, tooltip);
                        commandItem->setToolTip(1, tooltip);
                        break;
                    }
                    case CommandState::Removed: {
                        // Removed commands shown in gray
                        commandItem->setBackground(0, QColor(200, 200, 200)); // Light gray
                        commandItem->setBackground(1, QColor(200, 200, 200)); // Light gray
                        commandItem->setForeground(0, Qt::gray);
                        commandItem->setForeground(1, Qt::gray);
                        commandItem->setToolTip(0, "Command was removed from context");
                        commandItem->setToolTip(1, "Command was removed from context");
                        break;
                    }
                }

                // Store command ID in userData for potential future use
                commandItem->setData(0, Qt::UserRole, cmdInfo.id);

                // Add result as child item if it exists
                if (!cmdInfo.result.isEmpty()) {
                    QTreeWidgetItem* resultItem = new QTreeWidgetItem(commandItem);
                    resultItem->setText(0, "Result:");
                    resultItem->setText(1, cmdInfo.result);
                    resultItem->setBackground(0, QColor(240, 240, 240)); // Light gray background for result header
                    resultItem->setBackground(1, QColor(245, 245, 245)); // Light gray background
                    resultItem->setForeground(0, Qt::darkBlue);
                    resultItem->setForeground(1, Qt::darkBlue);

                    // Format the result in a more readable way if it's long
                    QStringList resultLines = cmdInfo.result.split('\n');
                    if (resultLines.size() > 10) {
                        // Show first 10 lines and indicate more
                        QStringList shortResult = resultLines.mid(0, 10);
                        shortResult << "... (click to expand)";
                        resultItem->setText(1, shortResult.join('\n'));
                    } else {
                        resultItem->setText(1, cmdInfo.result);
                    }
                }
            }
        }
    }

    // Expand only the top-level sequence/command items, keeping result items collapsed by default
    // This shows the structure but keeps the display compact
    for (int i = 0; i < m_contextCommandsList->topLevelItemCount(); ++i) {
        QTreeWidgetItem* topLevelItem = m_contextCommandsList->topLevelItem(i);
        topLevelItem->setExpanded(true);

        // Collapse any child items (results) by default, so they can be expanded when needed
        for (int j = 0; j < topLevelItem->childCount(); ++j) {
            QTreeWidgetItem* childItem = topLevelItem->child(j);
            // Keep child items (like results) collapsed by default
            childItem->setExpanded(false);
        }
    }

    if (m_contextGraphWidget) {
        m_contextGraphWidget->setGraphData(
            m_contextManager->getContextGraphNodes(),
            m_contextManager->activeContextGraphNodeId());
    }
}

void MainWindow::onFileChanged(const QString &path)
{
    qDebug() << "[MainWindow] File changed externally:" << path;

    // Remove the file from the watcher to avoid immediate re-triggering
    m_fileWatcher->removePath(path);

    // Add a small delay to allow file operations to complete
    QTimer::singleShot(100, this, [this, path]() {
        // Re-add the file to the watcher
        if (QFileInfo::exists(path)) {
            m_fileWatcher->addPath(path);
        }

        // Check if this file has context in our tracking
        if (m_fileToContextCommands.contains(path)) {
            qDebug() << "[MainWindow] Clearing context for file due to external change:" << path;

            // Clear contexts related to this file
            m_contextCache.clearContextForPath(path);

            // Also remove from file-to-commands mapping
            m_fileToContextCommands.remove(path);
        }

        // Inform ContextManager so command history is cleaned as well
        if (m_contextManager) {
            m_contextManager->cleanupForPath(path, QString("Context invalidated: %1 changed externally").arg(path));
        }
    });
}

void MainWindow::runTemplateRequested(const QString &templatePath, const QString &text)
{
    // This method is called when a specific template is selected from the combobox
    // We need to run the selected template with the provided text as the user query

    qDebug() << "[runTemplateRequested] Template path:" << templatePath;
    qDebug() << "[runTemplateRequested] Text:" << text;
    resetSequenceStabilityState();
    if (!m_preserveActiveImagesForNextRun) {
        m_activeImagesBase64.clear();
        m_activeImagePaths.clear();
        m_activeImageSummaries.clear();
    }
    m_preserveActiveImagesForNextRun = false;
    onChatTemplateSelectionChanged(templatePath);
    if (m_contextManager)
        m_contextManager->observeUserQuery(text);
    appendUserQuestionToResultDisplay(text);

    // First, refresh to ensure we have the latest templates and sequences
    m_templateMgr.refresh();
    refreshAllSequences(); // Refresh sequences including those in templates
    ensureDefaultSequence();

    // Find the template by path
    const QMap<QString, SolutionTemplate>& templates = m_templateMgr.templates();
    SolutionTemplate selectedTemplate;
    QString foundTemplatePath;

    for (auto it = templates.constBegin(); it != templates.constEnd(); ++it) {
        if (it.key() == templatePath) {
            selectedTemplate = it.value();
            foundTemplatePath = it.key();
            break;
        }
    }

    if (foundTemplatePath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Selected template not found: " + templatePath);
        return;
    }

    // Get the main sequence for this template
    QString mainSequencePath = selectedTemplate.mainSequencePath();

    // First, try to use the main sequence path as specified in the template file,
    // resolved relative to the template directory
    if (!mainSequencePath.isEmpty()) {
        QString resolvedMainSeqPath = QDir(foundTemplatePath).absoluteFilePath(mainSequencePath);
        if (QFile::exists(resolvedMainSeqPath)) {
            mainSequencePath = resolvedMainSeqPath;
        } else {
            // If the main sequence specified in template doesn't exist, fall back to first sequence
            auto sequenceNames = selectedTemplate.sequences();
            if (!sequenceNames.isEmpty()) {
                // Build the path to the first sequence file in the template directory
                mainSequencePath = QDir(foundTemplatePath).absoluteFilePath(sequenceNames[0]);
            } else {
                mainSequencePath = ""; // No sequences available
            }
        }
    } else {
        // If no main sequence is set in template, use the first available sequence
        auto sequenceNames = selectedTemplate.sequences();
        if (!sequenceNames.isEmpty()) {
            // Build the path to the first sequence file in the template directory
            mainSequencePath = QDir(foundTemplatePath).absoluteFilePath(sequenceNames[0]);
        }
    }

    if (mainSequencePath.isEmpty() || !QFile::exists(mainSequencePath)) {
        QMessageBox::warning(this, "Error", "No valid sequence found for template: " + selectedTemplate.name());
        return;
    }

    // Load the selected sequence
    PromptSequence seq;
    QFile seqFile(mainSequencePath);
    if (seqFile.open(QIODevice::ReadOnly)) {
        QByteArray data = seqFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            seq = PromptSequence::fromJson(doc.object());
        }
        seqFile.close();
    }

    if (seq.name().isEmpty()) {
        QMessageBox::warning(this, "Error", "Could not load the sequence for template: " + selectedTemplate.name());
        return;
    }

    // Set up the sequence to run and store the user query for use in templates
    m_currentSeq = seq;
    m_currentStepIdx = 0;
    m_sequenceMode = true;
    m_currentSeqPath = mainSequencePath; // Track the sequence path for related sequence lookup
    m_currentSeqId = m_contextManager->startCommandSequence(m_currentSeq.name(), m_currentSeq.description()); // Start tracking this sequence
    m_agentRepairCountThisStep = 0;
    m_agentRepairStepIdx = -1;
    m_agentRepairSeqId.clear();
    m_lastUserMessage = text; // Save the user's query for resolution in prompts

    updateSequenceGraphHighlight();  // Update the graph highlighting
    qDebug() << "[runTemplateRequested] About to start task timer for template execution";
    m_statusController->startTask();
    runNextStep();  // Start the sequence immediately
}


void MainWindow::runSimpleChat(const QString &text)
{
    // This method is called when "Простой чат" is selected - direct chat mode
    // Send the message directly to the model in streaming mode without templates/sequences

    qDebug() << "[runSimpleChat] Text:" << text;
    resetSequenceStabilityState();

    if (m_contextManager)
        m_contextManager->observeUserQuery(text);

    // Display user message in chat
    m_chat->appendUserMessage(text);
    appendUserQuestionToResultDisplay(text);

    // Start streaming response directly without any system context or templates
    m_streamBuf.clear();
    m_model.startStream(text);

    // Update history
    // Add to main history
    m_history.push_back(qMakePair(QString("user"), text));
    // Also add to separate user messages storage (for LAST_USER_MESSAGES resolver)
    m_lastUserMessages.append(text);
    // Keep only last N messages
    while (m_lastUserMessages.size() > MAX_USER_MESSAGES) {
        m_lastUserMessages.removeFirst();
    }

    // Log the request
    int tokens = m_promptMgr.tokenCount(text);
    log(QString("Sending simple chat request (%1 tokens) → %2").arg(tokens).arg(text));
}

static QString buildImagesNoteText(const QStringList &imagePaths, const QStringList &imageSummaries)
{
    if (imagePaths.isEmpty())
        return QString();

    QString out = "\n\n[Images]\n";
    for (int i = 0; i < imagePaths.size(); ++i) {
        const QString fileName = QFileInfo(imagePaths[i]).fileName();
        const QString summary = (i < imageSummaries.size()) ? imageSummaries[i].trimmed() : QString();
        if (!summary.isEmpty()) {
            out += QString("- %1: %2\n").arg(fileName, summary);
        } else {
            out += QString("- %1\n").arg(fileName);
        }
    }
    return out.trimmed();
}

static void addImagesToContextManager(ContextManager *ctx,
                                     const QStringList &imagePaths,
                                     const QStringList &imageSummaries)
{
    if (!ctx || imagePaths.isEmpty())
        return;

    class InlineImageContextCommand final : public Command {
    public:
        InlineImageContextCommand(const QString &name,
                                  const QJsonObject &params,
                                  const QString &desc,
                                  const QString &result)
            : m_name(name), m_params(params), m_desc(desc), m_result(result) {}

        QString name() const override { return m_name; }
        QJsonObject params() const override { return m_params; }
        QString description() const override { return m_desc; }
        bool apply(DevAssManager*, QString*) override { return true; }
        QJsonObject toJson() const override {
            QJsonObject o;
            o["command"] = m_name;
            o["params"] = m_params;
            o["description"] = m_desc;
            return o;
        }
        QString getResult() const override { return m_result; }

    private:
        QString m_name;
        QJsonObject m_params;
        QString m_desc;
        QString m_result;
    };

    for (int i = 0; i < imagePaths.size(); ++i) {
        const QString path = imagePaths[i];
        const QString fileName = QFileInfo(path).fileName();
        const QString summary = (i < imageSummaries.size()) ? imageSummaries[i].trimmed() : QString();

        QJsonObject params;
        params["path"] = path;
        if (!summary.isEmpty())
            params["summary"] = summary;

        const QString result = summary.isEmpty()
                                   ? QString("=== IMAGE: %1 ===\n%2").arg(fileName, path)
                                   : QString("=== IMAGE: %1 ===\n%2").arg(fileName, summary);
        InlineImageContextCommand cmd("USER_IMAGE", params, "User attached an image", result);
        ctx->addCommand(&cmd, result);
    }
}

void MainWindow::sendUserMessageWithImages(const QString &text,
                                          const QStringList &imagePaths,
                                          const QStringList &imagesBase64,
                                          const QStringList &imageSummaries)
{
    m_activeImagesBase64 = imagesBase64;
    m_activeImagePaths = imagePaths;
    m_activeImageSummaries = imageSummaries;
    addImagesToContextManager(m_contextManager, imagePaths, imageSummaries);
    sendUserMessage(text);
}

void MainWindow::runTemplateRequestedWithImages(const QString &templatePath,
                                               const QString &text,
                                               const QStringList &imagePaths,
                                               const QStringList &imagesBase64,
                                               const QStringList &imageSummaries)
{
    m_activeImagesBase64 = imagesBase64;
    m_activeImagePaths = imagePaths;
    m_activeImageSummaries = imageSummaries;
    m_preserveActiveImagesForNextRun = true;
    addImagesToContextManager(m_contextManager, imagePaths, imageSummaries);
    runTemplateRequested(templatePath, text);
}

void MainWindow::runSimpleChatWithImages(const QString &text,
                                        const QStringList &imagePaths,
                                        const QStringList &imagesBase64,
                                        const QStringList &imageSummaries)
{
    qDebug() << "[runSimpleChatWithImages] Text:" << text << "images:" << imagePaths.size();

    addImagesToContextManager(m_contextManager, imagePaths, imageSummaries);

    const QString note = buildImagesNoteText(imagePaths, imageSummaries);
    const QString historyText = note.isEmpty() ? text : (text + note);

    if (m_contextManager)
        m_contextManager->observeUserQuery(historyText);

    m_chat->appendUserMessage(note.isEmpty() ? text : (text + QString("\n[Images attached: %1]").arg(imagePaths.size())));
    appendUserQuestionToResultDisplay(historyText);

    m_streamBuf.clear();
    m_model.startStreamWithSystemAndImages(text, QString(), imagesBase64, QVariantMap());

    m_history.push_back(qMakePair(QString("user"), historyText));
    m_lastUserMessages.append(historyText);
    while (m_lastUserMessages.size() > MAX_USER_MESSAGES) {
        m_lastUserMessages.removeFirst();
    }

    int tokens = m_promptMgr.tokenCount(historyText);
    log(QString("Sending simple chat request (%1 tokens) -> %2").arg(tokens).arg(historyText));
}

void MainWindow::appendUserQuestionToResultDisplay(const QString& question)
{
    m_sequenceRunner->appendUserQuestionToResultDisplay(question);
}

ResultDisplayWidget::ResultEntryMetadata MainWindow::collectContextMetadataSince(const QDateTime &since) const
{
    ResultDisplayWidget::ResultEntryMetadata metadata;
    if (!m_contextManager || !since.isValid())
        return metadata;

    QList<CommandContextInfo> relevantCommands;
    QSet<QString> relevantIds;
    for (const auto &cmd : m_contextManager->getAllCommands()) {
        if (cmd.timestamp >= since) {
            relevantCommands << cmd;
            relevantIds.insert(cmd.id);
        }
    }

    if (relevantCommands.isEmpty())
        return metadata;

    metadata.commands = relevantCommands;

    for (const auto &seq : m_contextManager->getCommandSequences()) {
        bool include = seq.timestamp >= since;
        if (!include) {
            for (const QString &cmdId : seq.commandIds) {
                if (relevantIds.contains(cmdId)) {
                    include = true;
                    break;
                }
            }
        }
        if (include) {
            metadata.sequences << seq;
        }
    }

    return metadata;
}


void MainWindow::stopTemplateExecution()
{
    m_sequenceRunner->stopExecution();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    qDebug() << "[MainWindow] closeEvent called";

    // Save sequence graph layout before closing if it exists and is initialized
    if (m_seqGraphWindow) {
        qDebug() << "[MainWindow] Saving sequence graph layout";
        m_seqGraphWindow->saveLayout();
    } else {
        qDebug() << "[MainWindow] No sequence graph window to save";
    }

    // Clean up the sequence graph window
    if (m_seqGraphWindow) {
        delete m_seqGraphWindow;
        m_seqGraphWindow = nullptr;
    }

    QMainWindow::closeEvent(event);
}



void MainWindow::onOmniAsrText(const QString& text)
{
    if (text.isEmpty()) return;
    log("OmniASR Recognized: " + text);
    if (m_chat) {
        m_chat->setInputText(text);
        // Ensure chat is visible if needed, or just notify user
        if (!m_chat->isVisible()) {
             // Maybe show a toast or status message
             statusBar()->showMessage("ASR Text: " + text, 5000);
        }
    }
}
void MainWindow::updateChatToggleButton(bool visible)
{
    if (chatToggleBtn) {
        chatToggleBtn->setText(visible ? tr("Hide chat") : tr("Show chat"));
    }
}

