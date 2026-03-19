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
#include <QInputDialog>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTimer>
#include <QScrollArea>
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
    // Initialize member variables for task tracking
    m_taskTimer = nullptr;
    m_timerActive = false;
    m_taskRunning = false;
    m_totalElapsedTime = 0;

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

    // 3.3   Settings tab content
    m_settingsPanel = new QWidget(this);
    m_settingsPanel->setWindowTitle(tr("Settings"));
    m_settingsPanel->setAttribute(Qt::WA_DeleteOnClose, false);
    m_settingsPanel->setAttribute(Qt::WA_StyledBackground, true);
    m_settingsPanel->setObjectName("settingsPanel");
    m_settingsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_settingsPanel->setStyleSheet(
        "QWidget#settingsPanel {"
        "  background: #f8fffd;"
        "  border: 1px solid #86d8cc;"
        "  border-radius: 10px;"
        "}"
        "QWidget#settingsPanel QLabel[sectionTitle=\"true\"] {"
        "  color: #0f766e;"
        "  font: 600 9pt \"Segoe UI Variable\";"
        "  padding: 2px 0px;"
        "}"
        "QWidget#settingsPanel QFrame[sectionDivider=\"true\"] {"
        "  background: #bdebe4;"
        "  min-height: 1px;"
        "  max-height: 1px;"
        "  border: none;"
        "}"
    );
    QVBoxLayout *settingsLayout = new QVBoxLayout(m_settingsPanel);
    settingsLayout->setSpacing(7);
    settingsLayout->setContentsMargins(10, 10, 10, 10);
    settingsLayout->setSizeConstraint(QLayout::SetDefaultConstraint);

    auto addSettingsSection = [this, settingsLayout](const QString &title) {
        auto *section = new QLabel(title, m_settingsPanel);
        section->setProperty("sectionTitle", true);
        settingsLayout->addWidget(section);
    };
    auto addSettingsDivider = [this, settingsLayout]() {
        auto *line = new QFrame(m_settingsPanel);
        line->setFrameShape(QFrame::HLine);
        line->setProperty("sectionDivider", true);
        settingsLayout->addWidget(line);
    };

    addSettingsSection(tr("Project Path"));
    settingsLayout->addWidget(top_widget);
    addSettingsDivider();

    rescanBtn   = new StyledButton("Rescan", this);
    settingsBtn = new StyledButton("LLM Settings", this);
    promptBtn   = new StyledButton("Edit Base Prompt", this);
    applyBtn    = new StyledButton("Apply changes", this);
    undoBtn     = new StyledButton("Undo", this);
    descBtn     = new StyledButton("Generate Descriptions", this);
    addSettingsSection(tr("Project"));
    settingsLayout->addWidget(rescanBtn);
    settingsLayout->addWidget(settingsBtn);
    settingsLayout->addWidget(descBtn);
    addSettingsDivider();
    addSettingsSection(tr("Sequences"));

    runSeqBtn  = new StyledButton(tr("Запустить шаблон/последовательность"));
    //btns->addWidget(runSeqBtn);
    runSeqBtn->hide();
    StyledButton *runTemplateBtn = new StyledButton(tr("Запустить шаблон"), this);
    runTemplateBtn->hide();
    // settings panel layout placeholder to be added later

    editSeqBtn = new StyledButton(tr("Edit Sequences"), this);
    newSeqBtn = new StyledButton("New sequence...", this);
    settingsLayout->addWidget(editSeqBtn);
    settingsLayout->addWidget(newSeqBtn);

    connect(newSeqBtn, &StyledButton::clicked, this, &MainWindow::createNewSequence);

    // Connect the new template-only button
    connect(runTemplateBtn, &StyledButton::clicked, this, [this]() {
        // Directly open template mode
        refreshAllSequences();
        ensureDefaultSequence();

        RunTemplateOrSequenceDialog dlg(m_seqMgr.sequences(), &m_templateMgr, this);
        dlg.setTemplateModeOnly(); // Set to template mode
        if (dlg.exec() != QDialog::Accepted) return;

        if (dlg.selectedMode() != RunTemplateOrSequenceDialog::RunTemplate) {
            QMessageBox::warning(this, "Ошибка", "Необходимо выбрать шаблон.");
            return;
        }

        // Run template with selected sequence
        QString templatePath = dlg.selectedTemplatePath();
        QString sequencePath = dlg.selectedDefaultSequencePath();

        if (sequencePath.isEmpty() || !QFile::exists(sequencePath)) {
            QMessageBox::warning(this, "Ошибка", "Выбранная последовательность для шаблона не найдена.");
            return;
        }

        // Load the selected sequence
        PromptSequence seq;
        QFile seqFile(sequencePath);
        if (seqFile.open(QIODevice::ReadOnly)) {
            QByteArray data = seqFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                seq = PromptSequence::fromJson(doc.object());
            }
        }

        if (seq.name().isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Не удалось загрузить выбранную последовательность.");
            return;
        }

        m_currentSeq = seq;
        m_currentStepIdx = 0;
        m_sequenceMode = true;
        updateSequenceGraphHighlight();  // Update the graph highlighting
        runNextStep();   // сразу стартуем первый шаг
    });

    // Add a direct button for the main "Прогер" template
    StyledButton *runProgTemplateBtn = new StyledButton(tr("Запустить Прогер-шаблон"), this);
    runProgTemplateBtn->hide();
    //btns->addWidget(runProgTemplateBtn);

    connect(runProgTemplateBtn, &StyledButton::clicked, this, [this]() {
        refreshAllSequences();
        ensureDefaultSequence();

        // Look for the "Прогер" template specifically
        const QMap<QString, SolutionTemplate>& templates = m_templateMgr.templates();
        QString progTemplatePath;
        for (auto it = templates.constBegin(); it != templates.constEnd(); ++it) {
            if (it.value().name() == "Прогер" || it.value().name().contains("Прогер", Qt::CaseInsensitive)) {
                progTemplatePath = it.key();
                break;
            }
        }

        if (progTemplatePath.isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Шаблон 'Прогер' не найден. Пожалуйста, убедитесь, что он существует.");
            // Fallback to normal template selection
            RunTemplateOrSequenceDialog dlg(m_seqMgr.sequences(), &m_templateMgr, this);
            dlg.setTemplateModeOnly();
            if (dlg.exec() != QDialog::Accepted) return;

            QString templatePath = dlg.selectedTemplatePath();
            QString sequencePath = dlg.selectedDefaultSequencePath();

            if (sequencePath.isEmpty() || !QFile::exists(sequencePath)) {
                QMessageBox::warning(this, "Ошибка", "Выбранная последовательность для шаблона не найдена.");
                return;
            }

            PromptSequence seq;
            QFile seqFile(sequencePath);
            if (seqFile.open(QIODevice::ReadOnly)) {
                QByteArray data = seqFile.readAll();
                QJsonDocument doc = QJsonDocument::fromJson(data);
                if (doc.isObject()) {
                    seq = PromptSequence::fromJson(doc.object());
                }
            }

            if (seq.name().isEmpty()) {
                QMessageBox::warning(this, "Ошибка", "Не удалось загрузить выбранную последовательность.");
                return;
            }

            m_currentSeq = seq;
            m_currentStepIdx = 0;
            m_sequenceMode = true;
            m_currentSeqPath = sequencePath; // Track the sequence path for related sequence lookup
            m_currentSeqId = m_contextManager->startCommandSequence(m_currentSeq.name(), m_currentSeq.description()); // Start tracking this sequence
            updateSequenceGraphHighlight();
            runNextStep();
            return;
        }

        // Find the main sequence for the Прогер template
        SolutionTemplate progTemplate = templates.value(progTemplatePath);
        QString mainSequencePath = progTemplate.mainSequencePath();

        // First, try to use the main sequence path as specified in the template file,
        // resolved relative to the template directory
        if (!mainSequencePath.isEmpty()) {
            QString resolvedMainSeqPath = QDir(progTemplatePath).absoluteFilePath(mainSequencePath);
            if (QFile::exists(resolvedMainSeqPath)) {
                mainSequencePath = resolvedMainSeqPath;
            } else {
                // If the main sequence specified in template doesn't exist, fall back to first sequence
                auto sequences = progTemplate.sequences();
                if (!sequences.isEmpty()) {
                    // Get the first sequence in the template directory
                    mainSequencePath = QDir(progTemplatePath).absoluteFilePath(sequences[0]);
                } else {
                    mainSequencePath = ""; // No sequences available
                }
            }
        } else {
            // If no main sequence is set in template, use the first available sequence
            auto sequences = progTemplate.sequences();
            if (!sequences.isEmpty()) {
                // Get the first sequence in the template directory
                mainSequencePath = QDir(progTemplatePath).absoluteFilePath(sequences[0]);
            }
        }

        if (mainSequencePath.isEmpty() || !QFile::exists(mainSequencePath)) {
            QMessageBox::warning(this, "Ошибка", "Основная последовательность для шаблона 'Прогер' не найдена.");
            return;
        }

        // Load the main sequence
        PromptSequence seq;
        QFile seqFile(mainSequencePath);
        if (seqFile.open(QIODevice::ReadOnly)) {
            QByteArray data = seqFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                seq = PromptSequence::fromJson(doc.object());
            }
        }

        if (seq.name().isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Не удалось загрузить основную последовательность шаблона 'Прогер'.");
            return;
        }

        m_currentSeq = seq;
        m_currentStepIdx = 0;
        m_sequenceMode = true;
        m_currentSeqPath = mainSequencePath; // Track the sequence path for related sequence lookup
        m_currentSeqId = m_contextManager->startCommandSequence(m_currentSeq.name(), m_currentSeq.description()); // Start tracking this sequence
        updateSequenceGraphHighlight();
        runNextStep();
    });

    // Add button for solution templates
    StyledButton *editTemplatesBtn = new StyledButton(tr("Edit Templates"), this);
    settingsLayout->addWidget(editTemplatesBtn);

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

    // Add button to export selected sequence as template
    StyledButton *exportSequenceAsTemplateBtn = new StyledButton(tr("Export Sequence as Template"), this);
    settingsLayout->addWidget(exportSequenceAsTemplateBtn);

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

    // buttons already added to settings panel above
    addSettingsDivider();
    addSettingsSection(tr("Tools"));


    // ---------- Добавляем кнопку «Show Graph» ----------
    graphBtn = new StyledButton("Show Graph", this);
    settingsLayout->addWidget(graphBtn);
    connect(graphBtn, &StyledButton::clicked, this,
            &MainWindow::openGraph);

    // ---------- Добавляем кнопку «Show Sequence Graph» ----------
    seqGraphBtn = new StyledButton("Show Sequence Graph", this);
    settingsLayout->addWidget(seqGraphBtn);
    connect(seqGraphBtn, &StyledButton::clicked, this,
            &MainWindow::openSequenceGraph);

    // ---------- Добавляем кнопку «LightRAG» ----------
    lightRagBtn = new StyledButton("LightRAG", this);
    settingsLayout->addWidget(lightRagBtn);
    connect(lightRagBtn, &StyledButton::clicked, this,
            &MainWindow::openLightRag);
    // -------------------------------------------------

    // ---------- Добавляем кнопку «TTS» ----------
    // Load TTS state from settings (default: enabled)
    QSettings ttsSettings;
    m_ttsEnabled = ttsSettings.value("tts/enabled", true).toBool();
    
    m_ttsToggleBtn = new StyledButton(m_ttsEnabled ? tr("Озвучка: Вкл") : tr("Озвучка: Выкл"), this);
    settingsLayout->addWidget(m_ttsToggleBtn);
    
    // Initialize TTS if enabled
    if (m_ttsEnabled) {
        m_ttsRunner = new TtsRunner(this);
        connect(m_ttsRunner, &TtsRunner::statusMessage, this, [this](const QString& msg) {
            log("TTS: " + msg);
        });
        connect(m_ttsRunner, &TtsRunner::errorOccurred, this, [this](const QString& err) {
            log("TTS Error: " + err);
        });
        connect(m_ttsRunner, &TtsRunner::speakingFinished, this, [this]() {
            // Resume recording after TTS finishes speaking
            if (m_chat && m_chat->isRecording()) {
                m_chat->resumeRecordingAfterResponse();
            }
        });
    }
    
    connect(m_ttsToggleBtn, &StyledButton::clicked, this, [this]() {
        m_ttsEnabled = !m_ttsEnabled;
        m_ttsToggleBtn->setText(m_ttsEnabled ? tr("Озвучка: Вкл") : tr("Озвучка: Выкл"));
        
        // Save state to settings
        QSettings settings;
        settings.setValue("tts/enabled", m_ttsEnabled);
        
        if (m_ttsEnabled && !m_ttsRunner) {
            m_ttsRunner = new TtsRunner(this);
            connect(m_ttsRunner, &TtsRunner::statusMessage, this, [this](const QString& msg) {
                log("TTS: " + msg);
            });
            connect(m_ttsRunner, &TtsRunner::errorOccurred, this, [this](const QString& err) {
                log("TTS Error: " + err);
            });
            connect(m_ttsRunner, &TtsRunner::speakingFinished, this, [this]() {
                // Resume recording after TTS finishes speaking
                if (m_chat && m_chat->isRecording()) {
                    m_chat->resumeRecordingAfterResponse();
                }
            });
        }
    });
    // -------------------------------------------------


    addSettingsDivider();
    addSettingsSection(tr("Safety"));

    // Add confirmation mode combo box
    confirmModeCombo = new MemoryComboBox("confirmMode", this);
    confirmModeCombo->addItem("Auto-confirm commands", "auto");
    confirmModeCombo->addItem("Ask for command confirmation", "ask");
    confirmModeCombo->setCurrentIndex(0); // Default to auto-confirm
    settingsLayout->addWidget(confirmModeCombo);

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

    // Toggle button to show/hide chat
    chatToggleBtn = new StyledButton(tr("Show chat"), this);
    settingsLayout->addWidget(chatToggleBtn);
    connect(chatToggleBtn, &StyledButton::clicked, this, [this]() {
        const bool shouldShow = !m_chat->isChatLogVisible();
        m_chat->setChatLogVisible(shouldShow);
        chatToggleBtn->setText(shouldShow ? tr("Hide chat") : tr("Show chat"));
    });
    logToggleBtn = new StyledButton(tr("Show log"), this);
    settingsLayout->addWidget(logToggleBtn);
    connect(logToggleBtn, &StyledButton::clicked, this, [this]() {
        const bool shouldShow = !m_log->isVisible();
        m_log->setVisible(shouldShow);
        logToggleBtn->setText(shouldShow ? tr("Hide log") : tr("Show log"));
    });

    for (StyledButton *btn : m_settingsPanel->findChildren<StyledButton*>()) {
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setMinimumHeight(32);
    }
    settingsLayout->addStretch(1);

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

    QVBoxLayout* contextLayout = new QVBoxLayout(m_contextStatusWidget);
    contextLayout->setContentsMargins(5, 5, 5, 5);

    // Create header without toggle button
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel(tr("Context Commands:"), m_contextStatusWidget);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    contextLayout->addLayout(headerLayout);
    contextLayout->addWidget(m_contextCommandsList);

    // Set up the tree widget
    m_contextCommandsList->setHeaderLabels(QStringList() << "Time" << "Description");
    m_contextCommandsList->setColumnCount(2);
    m_contextCommandsList->setHeaderHidden(false);  // Show the header for better UX
    m_contextCommandsList->setRootIsDecorated(true); // Show expand/collapse icons
    m_contextCommandsList->setColumnWidth(0, 80);  // Set reasonable width for time column

    // Set size preference for the context status widget
    m_contextStatusWidget->setMinimumWidth(200);
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


    QWidget *assistantTab = new QWidget(this);
    auto *assistantTabLayout = new QVBoxLayout(assistantTab);
    assistantTabLayout->setContentsMargins(0, 0, 0, 0);
    assistantTabLayout->setSpacing(6);
    assistantTabLayout->addWidget(ide_widget, 1);

    auto *settingsScroll = new QScrollArea(this);
    settingsScroll->setWidgetResizable(true);
    settingsScroll->setFrameShape(QFrame::NoFrame);
    settingsScroll->setWidget(m_settingsPanel);

    QTabWidget *workspaceTabs = new QTabWidget(this);
    workspaceTabs->addTab(assistantTab, tr("Assistant"));
    workspaceTabs->addTab(settingsScroll, tr("Settings"));

    // --------  5. Формируем layout: слева context, по центру основной контент, справа graph ----------
    m_centerSplitter = new MemorySplitter("MainWindowsCenterSplitter", Qt::Vertical, this);
    m_centerSplitter->addWidget(workspaceTabs);
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

    // Задаём начальные размеры (опционально)
    setCentralWidget(mainSplitter);

    // --------  6. Соединяем сигналы / слоты ----------
    connect(browseBtn,   &StyledButton::clicked, this, &MainWindow::browse);
    connect(rescanBtn,   &StyledButton::clicked, this, &MainWindow::rescan);
    connect(settingsBtn, &StyledButton::clicked, this, &MainWindow::openSettings);
    connect(promptBtn,   &StyledButton::clicked, this, &MainWindow::editBasePrompt);
    connect(applyBtn,    &StyledButton::clicked, this, &MainWindow::applyChanges);
    connect(undoBtn,     &StyledButton::clicked, this, &MainWindow::undoLast);

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

    // Initialize task timer and status widget
    m_taskTimer = new QTimer(this);
    connect(m_taskTimer, &QTimer::timeout, this, &MainWindow::updateStatusDisplay);
    m_taskTimer->start(1000); // Update every second
    m_timerActive = false;

    createStatusWidget();
    updateStatusDisplay(); // Initial update

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
    qDebug() << "[MainWindow] openSequenceGraph called";

    // Refresh managers to ensure we have the latest data
    m_templateMgr.refresh();
    refreshAllSequences();

    ensureDefaultSequence();

    if (m_seqGraphWindow) {
        m_seqGraphWindow->setSequenceManager(&m_seqMgr);
        m_seqGraphWindow->setTemplateManager(&m_templateMgr);
        m_seqGraphWindow->rebuildGraph();
        m_seqGraphWindow->setFocus();
        m_seqGraphWindow->setVisible(true);
        updateSequenceGraphTemplateSelection();
    }
}

void MainWindow::onChatTemplateSelectionChanged(const QString &templatePath)
{
    QString normalizedPath = templatePath;
    if (normalizedPath == "simple_chat")
        normalizedPath.clear();

    if (m_currentTemplatePath == normalizedPath)
        return;

    m_currentTemplatePath = normalizedPath;
    updateSequenceGraphTemplateSelection();
}

void MainWindow::updateSequenceGraphHighlight()
{
    if (m_seqGraphWindow && m_sequenceMode) {
        m_seqGraphWindow->setCurrentSequence(m_currentSeq.name());
    }
}

void MainWindow::updateSequenceGraphTemplateSelection()
{
    if (m_seqGraphWindow) {
        m_seqGraphWindow->setTemplateSelectionByPath(m_currentTemplatePath);
    }
}

void MainWindow::openSequenceEditorForName(const QString &sequenceName)
{
    // Обновим кэш, чтобы включить все последовательности из папок шаблонов
    refreshAllSequences();

    // Find the sequence by name using the new method
    log("DEBUG: Searching for sequence name: " + sequenceName + " in openSequenceEditorForName");
    QPair<QString, PromptSequence> result = m_seqMgr.findSequenceByName(sequenceName);
    QString filePath = result.first;
    PromptSequence seq = result.second;

    if (filePath.isEmpty()) {
        // If not found in main manager, try to find in template directories
        log("DEBUG: Sequence NOT FOUND in main manager, searching in template directories...");

        // Search in template directories
        const QStringList& templateDirs = m_templateMgr.templateDirs();
        for (const QString& templateDir : templateDirs) {
            QDir dir(templateDir);
            if (dir.exists()) {
                // ищем *.seq.json в папке шаблона
                QStringList filters = {"*.seq.json"};
                QFileInfoList files = dir.entryInfoList(filters,
                                                        QDir::Files | QDir::NoSymLinks,
                                                        QDir::Name);

                for (const QFileInfo& fileInfo : files) {
                    QString path = fileInfo.absoluteFilePath();

                    // Load the sequence from this file and check its name
                    QFile seqFile(path);
                    if (seqFile.open(QIODevice::ReadOnly)) {
                        QByteArray data = seqFile.readAll();
                        QJsonDocument doc = QJsonDocument::fromJson(data);
                        if (doc.isObject()) {
                            PromptSequence tempSeq = PromptSequence::fromJson(doc.object());
                            if (tempSeq.name() == sequenceName) {
                                // Found the sequence in template directory
                                seq = tempSeq;
                                filePath = path;
                                log("DEBUG: Found sequence '" + sequenceName + "' in template directory: " + path);
                                seqFile.close();
                                break;
                            }
                        }
                        seqFile.close();
                    }
                }

                if (!filePath.isEmpty()) {
                    // Found in this template directory, exit the outer loop
                    break;
                }
            }
        }
    }

    if (filePath.isEmpty()) {
        log("DEBUG: Sequence NOT FOUND in openSequenceEditorForName. Available sequences:");
        const QMap<QString, PromptSequence>& allSequences_debug = m_seqMgr.sequences();
        for (auto it = allSequences_debug.constBegin(); it != allSequences_debug.constEnd(); ++it) {
            QString seqName = it.value().name();
            QString filePath = it.key();
            QFileInfo fileInfo(filePath);
            QString fileName = fileInfo.fileName();
            log("  - Available sequence name: '" + seqName + "' from file: '" + fileName + "'");
        }
        // Also list template directory contents
        const QStringList& templateDirs = m_templateMgr.templateDirs();
        log("  Template directories and their sequences:");
        for (const QString& templateDir : templateDirs) {
            QDir dir(templateDir);
            if (dir.exists()) {
                QStringList filters = {"*.seq.json"};
                QFileInfoList files = dir.entryInfoList(filters,
                                                        QDir::Files | QDir::NoSymLinks,
                                                        QDir::Name);
                log("    - Template directory: " + templateDir);
                for (const QFileInfo& fileInfo : files) {
                    // Load and get the sequence name
                    QFile seqFile(fileInfo.absoluteFilePath());
                    if (seqFile.open(QIODevice::ReadOnly)) {
                        QByteArray data = seqFile.readAll();
                        QJsonDocument doc = QJsonDocument::fromJson(data);
                        if (doc.isObject()) {
                            PromptSequence tempSeq = PromptSequence::fromJson(doc.object());
                            log("      - Sequence file: '" + fileInfo.fileName() + "' with internal name: '" + tempSeq.name() + "'");
                        }
                        seqFile.close();
                    }
                }
            }
        }
        QMessageBox::warning(this, "Error", "Sequence not found: " + sequenceName);
        return;
    } else {
        log("DEBUG: Successfully found sequence '" + sequenceName + "' at path: " + filePath);
    }

    openSequenceEditorWindow(tr("Редактор последовательности") + ": " + sequenceName, filePath, seq);
}

void MainWindow::openSequenceEditorWindow(const QString& title, const QString& filePath, const PromptSequence& seq)
{
    auto *win = new QWidget(this);
    win->setWindowFlag(Qt::Window, true);
    win->setAttribute(Qt::WA_DeleteOnClose, true);
    win->setWindowTitle(title);
    win->resize(1280, 820);
    win->setMinimumSize(1024, 700);

    auto *lay = new QVBoxLayout(win);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(10);

    auto *editor = new PromptSequenceEditor(win);
    editor->setSequenceManager(&m_seqMgr);
    editor->setSequence(seq);
    editor->setCurrentSequencePath(filePath);
    lay->addWidget(editor, 1);

    auto *btnRow = new QHBoxLayout();
    auto *saveBtn = new StyledButton(tr("Сохранить"), win);
    auto *closeBtn = new StyledButton(tr("Закрыть"), win);
    saveBtn->setEnabled(false);
    btnRow->addStretch(1);
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(closeBtn);
    lay->addLayout(btnRow);

    auto refreshAfterSave = [this]() {
        refreshAllSequences();
        ensureDefaultSequence();
        if (m_seqGraphWindow) {
            if (m_currentTemplatePath.isEmpty()) {
                m_seqGraphWindow->rebuildGraph();
            } else {
                m_seqGraphWindow->rebuildGraphForTemplate(m_currentTemplatePath);
                m_seqGraphWindow->setTemplateSelectionByPath(m_currentTemplatePath);
            }
        }
    };

    connect(editor, &PromptSequenceEditor::sequenceChanged, win, [win, saveBtn](const PromptSequence& edited) {
        saveBtn->setEnabled(true);
        win->setWindowTitle(QObject::tr("Редактор последовательности") + ": " + edited.name());
    });

    connect(saveBtn, &QPushButton::clicked, win, [this, editor, filePath, saveBtn, refreshAfterSave, win]() {
        const PromptSequence edited = editor->sequence();
        if (m_seqMgr.save(edited, filePath)) {
            QMessageBox::information(win, tr("Сохранено"), tr("Последовательность сохранена."));
            saveBtn->setEnabled(false);
            refreshAfterSave();
        } else {
            QMessageBox::warning(win, tr("Ошибка"), tr("Не удалось сохранить файл."));
        }
    });

    connect(closeBtn, &QPushButton::clicked, win, &QWidget::close);
    win->show();
    win->raise();
    win->activateWindow();
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
    if (!m_llmSettingsWindow) {
        m_llmSettingsWindow = new OllamaSettingsDialog(this);
        m_llmSettingsWindow->setModal(false);
        m_llmSettingsWindow->setWindowModality(Qt::NonModal);
        m_llmSettingsWindow->setAttribute(Qt::WA_DeleteOnClose, true);

        connect(m_llmSettingsWindow, &QObject::destroyed, this, [this]() {
            m_llmSettingsWindow = nullptr;
        });

        connect(m_llmSettingsWindow, &QDialog::accepted, this, [this]() {
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
    }

    m_llmSettingsWindow->show();
    m_llmSettingsWindow->raise();
    m_llmSettingsWindow->activateWindow();
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
    if (!m_contextContent.isEmpty())
        s += "\nСодержимое файлов, добавленных в контекст:\n" + m_contextContent + "\n";

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
    // Обновим кэш, чтобы включить все последовательности из папок шаблонов
    refreshAllSequences();

    // 1. Проверяем, что есть наборы
    if (m_seqMgr.sequences().isEmpty()) {
        QMessageBox::information(this, tr("Нет наборов"),
                                 tr("В папке нет ни одного *.seq.json"));
        return;
    }

    // 2. Формируем список названий файлов (без пути)
    QStringList names;
    QMap<QString, QString> pathByName;      // имя → путь
    for (const QString &filePath : m_seqMgr.fileList()) {
        QString name = QFileInfo(filePath).fileName();
        names << name;
        pathByName.insert(name, filePath);
    }

    // 3. Попросим пользователя выбрать файл
    bool ok = false;
    QString selectedName = QInputDialog::getItem(
                this, tr("Выберите последовательность"),
                tr("Файл:"), names, 0, false, &ok);

    if (!ok || selectedName.isEmpty())
        return;          // пользователь отменил

    QString filePath = pathByName.value(selectedName);

    // 4. Загружаем выбранную последовательность
    PromptSequence seq = m_seqMgr.sequences().value(filePath);

    openSequenceEditorWindow(tr("Редактор последовательности") + ": " + selectedName, filePath, seq);
}

void MainWindow::runSequenceDialog()
{
    qDebug() << "[runSequenceDialog] Starting sequence dialog";
    qDebug() << "[runSequenceDialog] Template manager root:" << m_templateMgr.root();
    qDebug() << "[runSequenceDialog] About to refresh template manager";

    // Обновляем списки (на случай, если пользователь добавил файлы вручную)
    m_templateMgr.refresh();
    qDebug() << "[runSequenceDialog] Template manager refresh completed";

    refreshAllSequences();
    qDebug() << "[runSequenceDialog] refreshAllSequences completed";

    ensureDefaultSequence();
    qDebug() << "[runSequenceDialog] ensureDefaultSequence completed";

    // Get sequences size before creating dialog
    // Make a copy of the sequences to avoid reference issues
    auto sequencesCopy = m_seqMgr.sequences();  // Copy instead of reference
    int seqCount = sequencesCopy.size();
    qDebug() << "[runSequenceDialog] Sequence count before dialog creation:" << seqCount;

    RunTemplateOrSequenceDialog dlg(sequencesCopy, &m_templateMgr, this);
    qDebug() << "[runSequenceDialog] Created RunTemplateOrSequenceDialog with" << seqCount << "sequences";

    if (dlg.exec() != QDialog::Accepted) {
        qDebug() << "[runSequenceDialog] Dialog cancelled";
        return;
    }

    qDebug() << "[runSequenceDialog] Dialog completed with selection";

    if (dlg.selectedMode() == RunTemplateOrSequenceDialog::RunSequence) {
        qDebug() << "[runSequenceDialog] Running individual sequence mode";
        // Run individual sequence - use sequence manager's sequences again to get fresh data
        const auto& currentSequences = m_seqMgr.sequences();
        QString path = dlg.selectedSequencePath();
        if (!currentSequences.contains(path)) {
            QMessageBox::warning(this, "Ошибка", "Файл последовательности не найден.");
            return;
        }

        m_currentSeq = currentSequences.value(path);
        m_currentStepIdx = 0;
        m_sequenceMode = true;
        m_currentSeqPath = path; // Track the sequence path for related sequence lookup
        m_currentSeqId = m_contextManager->startCommandSequence(m_currentSeq.name(), m_currentSeq.description()); // Start tracking this sequence
        updateSequenceGraphHighlight();  // Update the graph highlighting
        qDebug() << "[runSequenceDialog] About to start task timer for sequence execution";
        startTaskTimer(); // Start the task timer when sequence begins
        qDebug() << "[runSequenceDialog] Starting sequence:" << m_currentSeq.name();
        runNextStep();   // сразу стартуем первый шаг
    } else {
        qDebug() << "[runSequenceDialog] Running template mode";
        // Run template with selected sequence
        QString templatePath = dlg.selectedTemplatePath();
        QString sequencePath = dlg.selectedDefaultSequencePath();

        if (sequencePath.isEmpty() || !QFile::exists(sequencePath)) {
            QMessageBox::warning(this, "Ошибка", "Выбранная последовательность для шаблона не найдена.");
            return;
        }

        // Load the selected sequence
        PromptSequence seq;
        QFile seqFile(sequencePath);
        if (seqFile.open(QIODevice::ReadOnly)) {
            QByteArray data = seqFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                seq = PromptSequence::fromJson(doc.object());
            }
        }

        if (seq.name().isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Не удалось загрузить выбранную последовательность.");
            return;
        }

        m_currentSeq = seq;
        m_currentStepIdx = 0;
        m_sequenceMode = true;
        updateSequenceGraphHighlight();  // Update the graph highlighting
        qDebug() << "[runSequenceDialog] About to start task timer for template execution";
        startTaskTimer(); // Start the task timer when sequence begins
        runNextStep();   // сразу стартуем первый шаг
    }
}

void MainWindow::runNextStep()
{
    qDebug() << "[runNextStep] m_sequenceMode:" << m_sequenceMode << "m_currentStepIdx:" << m_currentStepIdx;
    if (!m_sequenceMode) return;

    // Check if stop was requested
    if (m_stopRequested) {
        // Reset the stop flag and exit sequence mode
        m_stopRequested = false;
        m_sequenceMode = false;
        m_currentSeqId.clear(); // Clear the current sequence ID
        updateSequenceGraphHighlight();  // Update the graph highlighting (will clear highlight)

        // Show message that execution was stopped
        QMessageBox::information(this, "Остановлено",
                                 "Выполнение шаблона остановлено пользователем.");
        return;
    }

    if (m_currentStepIdx < 0 ||
            m_currentStepIdx >= m_currentSeq.steps().size()) {
        // конец последовательности
        QMessageBox::information(this, "Готово",
                                 "Последовательность выполнена полностью.");
        // End the command sequence if we were tracking one
        if (!m_currentSeqId.isEmpty() && m_contextManager) {
            m_contextManager->endCommandSequence(m_currentSeqId);
        }
        m_sequenceMode = false;
        m_currentSeqId.clear(); // Clear the current sequence ID
        stopTaskTimer(); // Stop the task timer when sequence completes
        updateSequenceGraphHighlight();  // Update the graph highlighting (will clear highlight)
        return;
    }

    const PromptStep &step = m_currentSeq.steps()[m_currentStepIdx];
    PromptResolver::ResolvedPrompt resolved = PromptResolver::resolve(step.tmpl(), step, this);
    qDebug() << "[runNextStep] systemPrompt snippet:" << resolved.systemPrompt;
    qDebug() << "[runNextStep] userPrompt:" << resolved.userPrompt;

    // Оценка токенов для контекста шага
    {
        int tokens = m_model.estimateTokens(resolved.systemPrompt + "\n" + resolved.userPrompt);
        log(QString("[Шаг %1] Контекст ≈ %2 токенов").arg(m_currentStepIdx+1).arg(tokens));
    }
    // Логируем, показываем полъзователъ
    log(QString("\n--- Шаг %1 ------------------------------------------------")
        .arg(m_currentStepIdx+1));
    log(resolved.systemPrompt);
    if (!resolved.userPrompt.isEmpty())
        log(QString("USER_QUERY:\n%1").arg(resolved.userPrompt));

    // Add the resolved user prompt to the separate user messages storage (for LAST_USER_MESSAGES resolver)
    // Only add if it's not already the most recent message to prevent duplicates
    if (!resolved.userPrompt.isEmpty()) {
        if (m_lastUserMessages.isEmpty() || m_lastUserMessages.last() != resolved.userPrompt) {
            m_history.push_back(qMakePair(QString("user"), resolved.userPrompt));
            m_lastUserMessages.append(resolved.userPrompt);
            // Keep only last N messages
            while (m_lastUserMessages.size() > MAX_USER_MESSAGES) {
                m_lastUserMessages.removeFirst();
            }
            qDebug() << "[runNextStep] Added user prompt to LAST_USER_MESSAGES storage. Current count:" << m_lastUserMessages.size();
        } else {
            qDebug() << "[runNextStep] Skipping duplicate user prompt in LAST_USER_MESSAGES storage";
        }
    }

    // Отправляем запрос так же, как обычный ввод полъзователъ
    m_streamBuf.clear();

    // Добавляем отладочный вывод, чтобы понять, что отправляем в LLM
    qDebug() << "[DEBUG SEND TO LLM] - USER PROMPT:" << resolved.userPrompt;
    qDebug() << "[DEBUG SEND TO LLM] - SYSTEM PROMPT (first 500 chars):" << resolved.systemPrompt.left(500);
    qDebug() << "[DEBUG SEND TO LLM] - SYSTEM PROMPT LENGTH:" << resolved.systemPrompt.length();

    m_currentPrompt = resolved.systemPrompt + "\n" + resolved.userPrompt;  // Store the current prompt

    // Reset per-step repair counter when step/sequence changes.
    if (m_agentRepairStepIdx != m_currentStepIdx || m_agentRepairSeqId != m_currentSeqId) {
        m_agentRepairCountThisStep = 0;
        m_agentRepairStepIdx = m_currentStepIdx;
        m_agentRepairSeqId = m_currentSeqId;
    }

    if (m_useAgentPipeline && ensureAgentModelConfigured()) {
        // Multi-pass: RAW draft (logged) -> compiled JSON commands.
        QVariantMap draftOpts;
        draftOpts.insert("temperature", 0.5);
        draftOpts.insert("num_predict", 600);
        draftOpts.insert("timeoutMs", 60000);

        const QString draftSystem =
            QStringLiteral(
                "You are a senior planner.\n"
                "Return a short plan (5-10 bullets) and, if needed, 1-3 clarifying questions.\n"
                "Do NOT output JSON commands in this step.\n");

        const QString rawDraft = m_agentModel.generateWithSystem(resolved.userPrompt, draftSystem, draftOpts).trimmed();
        if (!rawDraft.isEmpty())
            appendConsoleContext(QStringLiteral("AGENT_RAW_DRAFT"), rawDraft, QString(), 0);

        QVariantMap compileOpts;
        compileOpts.insert("temperature", 0.1);
        compileOpts.insert("num_predict", 1400);
        compileOpts.insert("timeoutMs", 90000);

        QString compileSystem = resolved.systemPrompt;
        if (!rawDraft.isEmpty()) {
            compileSystem += QStringLiteral("\n\n[RAW_DRAFT]\n") + rawDraft + QStringLiteral("\n\n");
        }
        compileSystem += QStringLiteral("\n\nIMPORTANT: Output ONLY a JSON array of commands. No extra text.\n");

        const QString compiled = m_agentModel.generateWithSystem(resolved.userPrompt, compileSystem, compileOpts);
        QTimer::singleShot(0, this, [this, compiled]() {
            m_streamBuf = compiled;
            handleAssistantFinished();
        });
        return;
    }

    // Fallback: stream from the currently selected model.
    if (!m_activeImagesBase64.isEmpty()) {
        m_model.startStreamWithSystemAndImages(resolved.userPrompt, resolved.systemPrompt, m_activeImagesBase64, QVariantMap());
    } else {
        m_model.startStreamWithSystem(resolved.userPrompt, resolved.systemPrompt);
    }
    // В истории сохраняем только запрос (не пользовательский текст)
    //m_history.push_back(qMakePair(QString("system"), resolved));
}
/*
void MainWindow::handleAssistantFinished()
{
    // 1. Ответ LLM – полный текст (включая «лишние» строки)
    QByteArray fullResp = m_streamBuf.toUtf8();

    // 2. Выбираем только JSON‑часть
    QByteArray jsonResp = extractJsonPart(fullResp);

    // 3. Проверяем, что JSON найден
    if (jsonResp.isEmpty()) {
        log("Assistant ответил, но не содержит JSON-массив. Команды не применяются.");
        log(QString("Сырой ответ модели:\n%1").arg(QString::fromUtf8(fullResp)));
        const QJsonObject lastBody = m_model.lastRequestBody();
        if (!lastBody.isEmpty()) {
            log(QString("Последний запрос к Ollama:\n%1")
                    .arg(QString::fromUtf8(QJsonDocument(lastBody).toJson(QJsonDocument::Indented))));
        }
        //return;
    }

    m_history.push_back(qMakePair(QString("assistant"), QString(fullResp)));

    // 4. Парсим команды
    QVector<Command*> cmds = CommandParser::parse(jsonResp);

    // ... остальная часть функции остаётся без изменений ...
    //m_history.push_back(qMakePair(QString("<assistant>"), QString(fullResp)));

    if (cmds.isEmpty()) {
        log("LLM не вернул ни одной команды");
        return;
    }

    // 5. Подтверждение пользователем
    CommandConfirmDialog dlg(cmds.toList(), this);
    if (dlg.exec() != QDialog::Accepted) {
        log("Пользователь отклонил команды");
        qDeleteAll(cmds);
        return;
    }
    QList<Command*> toRun = dlg.confirmed();

    // 6. Обработка специальных команд (без изменений на диске)
    for (Command* c : toRun) {
        if (c->name()=="ASK_USER") {
            QString q = c->params().value("question").toString();
            bool ok=false; QString ans = QInputDialog::getText(this, tr("Ответьте на вопрос нейросети"), q, QLineEdit::Normal, QString(), &ok);
            if (ok) {
                m_history.push_back(qMakePair(QString("user"), ans));
                // Also add to separate user messages storage (for LAST_USER_MESSAGES resolver)
                m_lastUserMessages.append(ans);
                // Keep only last N messages
                while (m_lastUserMessages.size() > MAX_USER_MESSAGES) {
                    m_lastUserMessages.removeFirst();
                }
            }
        } else if (c->name()=="OPEN_FILE_REGION" || c->name()=="OPEN_FILE") {
            QJsonObject p = c->params(); QString path = p.value("path").toString(); QString abs = QDir(m_projectRoot).filePath(path);
            QFile f(abs); if (f.open(QIODevice::ReadOnly)) {
                QString txt = QString::fromUtf8(f.readAll()); f.close();
                QString snippet;
                if (c->name()=="OPEN_FILE_REGION") {
                    int sLine = p.value("startLine").toInt(-1); int eLine = p.value("endLine").toInt(-1);
                    QString region = p.value("region").toString();
                    if (sLine>=1 && eLine>=sLine) {
                        QStringList lines = txt.split("\n");
                        sLine = qMax(1, sLine); eLine = qMin(eLine, lines.size());
                        snippet = lines.mid(sLine-1, eLine-sLine+1).join("\n");
                    } else if (!region.isEmpty()) {
                        QRegularExpression re(region, QRegularExpression::DotMatchesEverythingOption|QRegularExpression::MultilineOption);
                        auto m = re.match(txt);
                        if (m.hasMatch()) snippet = m.captured(0);
                    } else {
                        snippet = txt;
                    }
                } else { snippet = txt; }
                if (!snippet.isEmpty()) {
                    m_contextContent += QString("\n==== %1 ====%2\n%3\n").arg(path).arg(c->name()=="OPEN_FILE_REGION"?" (region)":"").arg(snippet);
                }
            }
        }
    }

    // 6. Применяем команды

    // Сводка для PREVIOUS_STEPS
    QStringList appliedSummary;
    for (Command* c : toRun) {
        QString one = c->name();
        QJsonObject p = c->params();
        if (p.contains("path")) one += QString(" path=%1").arg(p.value("path").toString());
        if (p.contains("from") && p.contains("to")) one += QString(" %1→%2").arg(p.value("from").toString(), p.value("to").toString());
        appliedSummary << one;
    }
    if (!appliedSummary.isEmpty()) {
        m_prevSteps << QString("Шаг %1: %2").arg(m_currentStepIdx+1).arg(appliedSummary.join("; "));
    }

    QString err;
    for (Command* c : toRun) {
        // Для команд, требующих LLM, используем applyWithLLM
        if (!c->applyWithLLM(&m_dev_manager, execModel, &err)) {
            QMessageBox::warning(this, "Ошибка выполнения", err);
            break;
        }

    }

    // 7. После применения обновляем список
    rescan();


    // После выполнения всех подтверждённых команд:
    rescan(); // обновляем UI

    // Если мы в режиме последовательности – переходим к следующему шагу
    if (m_sequenceMode) {
        ++m_currentStepIdx;
        QTimer::singleShot(200, this, &MainWindow::runNextStep); // небольшая пауза
    }


    qDeleteAll(toRun);
}
*/

void MainWindow::handleAssistantFinished()
{
    // Check if stop was requested - if so, don't continue with any processing
    if (m_stopRequested) {
        m_stopRequested = false;
        m_sequenceMode = false;
        m_currentSeqId.clear(); // Clear the current sequence ID
        updateSequenceGraphHighlight();  // Update the graph highlighting (will clear highlight)

        // Show message that execution was stopped
        QMessageBox::information(this, "Остановлено",
                                 "Выполнение шаблона остановлено пользователем.");
        stopTaskTimer(); // Stop the task timer when execution is stopped
        qDeleteAll(CommandParser::parse(m_streamBuf.toUtf8()));
        return;
    }

    QByteArray fullResp = m_streamBuf.toUtf8();
    QByteArray jsonResp = extractJsonPart(fullResp);

    // If the model failed to output JSON, attempt a strict repair pass (sequence mode only).
    if (jsonResp.isEmpty() && m_sequenceMode &&
        m_currentStepIdx >= 0 && m_currentStepIdx < m_currentSeq.steps().size()) {
        const PromptStep &step = m_currentSeq.steps()[m_currentStepIdx];
        const QByteArray repaired = repairToJsonArrayIfNeeded(fullResp, step);
        if (!repaired.isEmpty()) {
            jsonResp = repaired;
        }
    }

    const bool usedRaw = jsonResp.isEmpty();
    const QByteArray payload = usedRaw ? fullResp : jsonResp;
    if (usedRaw) {
        appendConsoleContext("LLM RAW OUTPUT", QString::fromUtf8(fullResp), QString(), -1);
        log("LLM ответ сохранён как контекст: не удалось извлечь JSON. Пытаемся распарсить сырой ответ.");
    }

    m_history.push_back(qMakePair(QString("assistant"), QString::fromUtf8(payload)));
    m_lastAssistantMessage = QString::fromUtf8(payload);

    // Check if the LLM specified to open a new sequence
    QString nextSeqName = PromptResolver::getNextSequence();
    if (!nextSeqName.isEmpty()) {
        log("DEBUG: NEXT_SEQUENCE requested, searching for: " + nextSeqName);
        // Refresh all sequences to ensure latest templates are loaded
        refreshAllSequences();

        const QMap<QString, PromptSequence>& allSequences = m_seqMgr.sequences();
        PromptSequence foundSeq;
        QString foundPath;

        // First try to find by internal sequence name
        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            if (it.value().name() == nextSeqName) {
                foundSeq = it.value();
                foundPath = it.key();
                log("Found sequence by name: " + nextSeqName + " at " + foundPath); // Debug log
                break;
            }
        }

        // If not found by name, try by filename
        if (foundPath.isEmpty()) {
            for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
                QFileInfo fileInfo(it.key());
                if (fileInfo.fileName() == nextSeqName) {
                    foundSeq = it.value();
                    foundPath = it.key();
                    log("Found sequence by filename: " + nextSeqName + " at " + foundPath); // Debug log
                    break;
                }
            }
        }

        // Try with expected extension if not already present
        if (foundPath.isEmpty()) {
            QString expectedFileName = nextSeqName;
            if (!expectedFileName.endsWith(".seq.json")) {
                expectedFileName += ".seq.json";
            }
            for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
                QFileInfo fileInfo(it.key());
                if (fileInfo.fileName() == expectedFileName) {
                    foundSeq = it.value();
                    foundPath = it.key();
                    log("Found sequence by expected filename: " + expectedFileName + " at " + foundPath); // Debug log
                    break;
                }
            }
        }

        // Try base name match (without extension)
        if (foundPath.isEmpty()) {
            for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
                QFileInfo fileInfo(it.key());
                if (fileInfo.baseName() == nextSeqName) {
                    foundSeq = it.value();
                    foundPath = it.key();
                    log("Found sequence by basename match: " + nextSeqName + " at " + foundPath); // Debug log
                    break;
                }
            }
        }

        // Try base name matching for .seq extension case
        if (foundPath.isEmpty()) {
            for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
                QFileInfo fileInfo(it.key());
                if (fileInfo.baseName() == nextSeqName + ".seq") {
                    foundSeq = it.value();
                    foundPath = it.key();
                    log("Found sequence by basename + .seq match: " + nextSeqName + ".seq" + " at " + foundPath); // Debug log
                    break;
                }
            }
        }

        // If still not found and we're currently in a sequence from a template,
        // try to find the sequence file in the same directory as the current sequence
        if (foundPath.isEmpty() && !m_currentSeqPath.isEmpty()) {
            QFileInfo currentSeqInfo(m_currentSeqPath);
            QDir templateDir = currentSeqInfo.absoluteDir();

            // Try multiple possible formats for the sequence filename
            QStringList possibleFilenames;

            // Add the exact name as provided
            possibleFilenames << nextSeqName;

            // Add with .seq.json extension if not already present
            if (!nextSeqName.endsWith(".seq.json")) {
                possibleFilenames << (nextSeqName + ".seq.json");
            }

            // Add with just .json extension if not already present
            if (!nextSeqName.endsWith(".json")) {
                possibleFilenames << (nextSeqName + ".json");
            }

            // Add with just .seq extension if not already present
            if (!nextSeqName.endsWith(".seq")) {
                possibleFilenames << (nextSeqName + ".seq");
            }

            // Try each possible filename in the template directory
            for (const QString& filename : possibleFilenames) {
                QString sequenceFilePath = templateDir.filePath(filename);

                qDebug()<<"templateDir === "<<templateDir;

                if (QFile::exists(sequenceFilePath)) {
                    // Load the sequence from this file
                    QFile seqFile(sequenceFilePath);
                    if (seqFile.open(QIODevice::ReadOnly)) {
                        QByteArray data = seqFile.readAll();
                        QJsonDocument doc = QJsonDocument::fromJson(data);
                        if (doc.isObject()) {
                            foundSeq = PromptSequence::fromJson(doc.object());
                            foundPath = sequenceFilePath;
                            log(QString("Found sequence in template directory: %1").arg(sequenceFilePath)); // Debug log
                            break;
                        }
                        seqFile.close();
                    }
                }

                if (!foundPath.isEmpty()) break; // Found, exit the loop
            }
        }

        if (!foundPath.isEmpty()) {
            m_currentSeq = foundSeq;
            m_currentStepIdx = 0;
            m_sequenceMode = true;
            m_currentSeqPath = foundPath; // Update the sequence path when transitioning via template-based OPEN_SEQUENCE
            addSequenceTransitionToHistory(nextSeqName);  // Add transition to history
            updateSequenceGraphHighlight();  // Update the graph highlighting
            PromptResolver::clearNextSequence();  // Clear for next usage
            QTimer::singleShot(200, this, &MainWindow::runNextStep);
            qDeleteAll(CommandParser::parse(payload));  // Clean up parsed commands
            return;
        }

        // If sequence not found, log error and continue with normal flow
        log("Указанная последовательность не найдена: " + nextSeqName);
        // Add extensive debugging to identify what sequences are available
        log("DEBUG: NEXT_SEQUENCE - Available sequences debugging:");
        log("  Total sequences in manager: " + QString::number(allSequences.size()));
        log("  Searched sequence name: '" + nextSeqName + "'");
        log("  Available sequence names and paths:");
        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            QString seqName = it.value().name();
            QString filePath = it.key();
            QFileInfo fileInfo(filePath);
            QString fileName = fileInfo.fileName();
            QString baseName = fileInfo.baseName();
            log("    - Sequence name: '" + seqName + "'");
            log("    - File name: '" + fileName + "'");
            log("    - Base name: '" + baseName + "'");
            log("    - File path: " + filePath);
            log("    - Match details: name='" + seqName + "', filename='" + fileName + "', basename='" + baseName + "'");
        }
        PromptResolver::clearNextSequence();
    }

    QVector<Command*> cmds = CommandParser::parse(payload);
    if (cmds.isEmpty()) {
        if (!usedRaw)
            appendConsoleContext("LLM RAW OUTPUT", QString::fromUtf8(payload), QString(), -1);
        log("LLM не вернул команд. Сырой ответ добавлен в контекст.");

        // В режиме последовательности - переходим к следующему шагу, даже если команд нет
        if (m_sequenceMode) {
            // Check if stop was requested before advancing to next step
            if (m_stopRequested) {
                m_stopRequested = false;
                m_sequenceMode = false;
                m_currentSeqId.clear(); // Clear the current sequence ID
                updateSequenceGraphHighlight();  // Update the graph highlighting (will clear highlight)

                // Show message that execution was stopped
                QMessageBox::information(this, "Остановлено",
                                         "Выполнение шаблона остановлено пользователем.");
                return;
            }
            ++m_currentStepIdx;
            QTimer::singleShot(200, this, &MainWindow::runNextStep);
        } else {
            retryCurrentSequenceStep();
        }
        return;
    }

    QList<Command*> toRun;
    // Check confirmation mode
    if (confirmModeCombo->currentData().toString() == "auto") {
        // Auto-confirm mode: All commands are confirmed by default
        toRun = cmds.toList();

        // Log that commands are running without confirmation
        log(QString("Команды запускаются без подтверждения (режим: %1).").arg(confirmModeCombo->currentText()));
    } else {
        // Ask for confirmation mode - but check all commands by default so user doesn't need to click each
        CommandConfirmDialog dlg(cmds.toList(), this, true); // Check all by default in "ask" mode
        if (dlg.exec() != QDialog::Accepted) {
            log("Пользователь отклонил команды");
            qDeleteAll(cmds);
            return;
        }
        toRun = dlg.confirmed();
    }

    bool hasCallSequenceCommand = false;
    bool hasOpenSequenceCommand = false;
    QString sequenceToCall;
    QString openSequenceName;

    for (Command* c : toRun) {
        if (c->name() == "ASK_USER") {
            const QString question = c->params().value("question").toString();
            bool ok = false;
            const QString ans = QInputDialog::getText(this,
                                                      tr("Ответьте на вопрос нейросети"),
                                                      question,
                                                      QLineEdit::Normal,
                                                      QString(),
                                                      &ok);
            if (ok) {
                m_history.push_back(qMakePair(QString("user"), ans));
                // Also add to separate user messages storage (for LAST_USER_MESSAGES resolver)
                m_lastUserMessages.append(ans);
                // Keep only last N messages
                while (m_lastUserMessages.size() > MAX_USER_MESSAGES) {
                    m_lastUserMessages.removeFirst();
                }
            }
        } else if (c->name() == "OPEN_FILE_REGION" || c->name() == "OPEN_FILE") {
            handleOpenCommand(c);
        } else if (c->name() == "SHOW_RESULT") {
            QString title = c->params().value("title").toString();
            if (title.isEmpty())
                title = tr("Ответ от %1:\n\n").arg(m_model.selectedModel());
            QString content = c->params().value("content").toString() + "\n\n\n";
            if (content.isEmpty())
                content = tr("SHOW_RESULT вызван без текста.\n\n\n");

            // Add the result to the new result display widget
            if (m_resultDisplayWidget) {
                const QString displayTitle = tr("Ответ нейросети");
                ResultDisplayWidget::ResultEntryMetadata metadata = collectContextMetadataSince(m_lastQuestionTimestamp);
                m_resultDisplayWidget->appendResult(displayTitle, content,
                                                    ResultDisplayWidget::EntryType::Default,
                                                    metadata);
            } else {
                // Fallback: show in external context as before
                appendExternalContext(QString("==== SHOW_RESULT ====\n%1").arg(content));
            }

            // Speak the SHOW_RESULT content if TTS is enabled
            if (m_ttsEnabled && m_ttsRunner && m_ttsRunner->isReady()) {
                QString textToSpeak = c->params().value("content").toString();
                textToSpeak.replace("\\n", " ").replace("\\r", "").replace("\n", " ");
                // Limit length for TTS
                if (textToSpeak.length() > 1000) {
                    textToSpeak = textToSpeak.left(1000) + "...";
                }
                if (!textToSpeak.isEmpty() && textToSpeak.length() > 5) {
                    m_ttsRunner->speak(textToSpeak);
                }
            }

            // Add command to context manager with result
            if (m_contextManager) {
                addCommandToContext(c, c->getResult());
            }
        } else if (c->name() == "REASONING") {
            // For REASONING command, we simply add the reasoning content to history
            // so it becomes part of the conversation context, just like other commands' results
            QString reasoning = c->params().value("reasoning").toString();
            if (reasoning.isEmpty()) {
                reasoning = c->params().value("thought_process").toString();
            }
            if (reasoning.isEmpty()) {
                reasoning = c->params().value("reasoning_text").toString();
            }

            // Add the reasoning to the result display widget with special styling
            if (!reasoning.isEmpty()) {
                if (m_resultDisplayWidget) {
                QString formattedReasoning = reasoning;
                ResultDisplayWidget::ResultEntryMetadata metadata = collectContextMetadataSince(m_lastQuestionTimestamp);
                m_resultDisplayWidget->appendResult("", formattedReasoning,
                                                    ResultDisplayWidget::EntryType::Reasoning,
                                                    metadata);
                } else {
                    // Fallback: add to history as system message
                    m_history.push_back(qMakePair(QString("system"), reasoning));
                }
            }

            // Add command to context manager with reasoning as result
            if (m_contextManager) {
                addCommandToContext(c, c->getResult());
            }
        } else if (c->name() == "CONTEXT_SHOW") {
            rebuildContextCache();
            int maxChars = c->params().value("maxChars").toInt(8000);
            if (maxChars <= 0) maxChars = 0;
            QString ctx = m_contextContent;
            if (maxChars > 0 && ctx.size() > maxChars) {
                ctx = ctx.left(maxChars) + "\n[...truncated...]";
            }

            const QString result = QString("CONTEXT_SHOW\n[context]\n%1").arg(ctx.trimmed());

            if (m_resultDisplayWidget) {
                ResultDisplayWidget::ResultEntryMetadata metadata = collectContextMetadataSince(m_lastQuestionTimestamp);
                m_resultDisplayWidget->appendResult(tr("Context"), result,
                                                    ResultDisplayWidget::EntryType::Default,
                                                    metadata);
            } else {
                appendExternalContext(QString("==== CONTEXT_SHOW ====\n%1").arg(ctx));
            }

            m_history.push_back(qMakePair(QString("system"), result));
            if (m_contextManager) {
                addCommandToContext(c, result);
            }
        } else if (c->name() == "CONTEXT_CLEAR") {
            const QString scope = c->params().value("scope").toString().trimmed().toLower();

            // Clear file-based context cache
            m_contextSlices.clear();
            m_fileToContextCommands.clear();
            if (m_fileWatcher)
                m_fileWatcher->removePaths(m_fileWatcher->files());

            // Clear console/external context caches
            m_consoleLogs.clear();
            m_externalSources.clear();
            m_shellCommandOutputs.clear();
            m_pythonScriptOutputs.clear();

            if (m_contextManager) {
                if (scope == "all")
                    m_contextManager->clearAll();
                else
                    m_contextManager->clearActiveContextOnly();
            }

            rebuildContextCache();

            const QString result = QString("CONTEXT_CLEAR\n[scope]\n%1\n[status]\nOK").arg(scope.isEmpty() ? "active" : scope);
            m_history.push_back(qMakePair(QString("system"), result));
            if (m_contextManager) {
                addCommandToContext(c, result);
            }
        } else if (c->name() == "CALL_SEQUENCE") {
            // Mark that we have a sequence call command and store the name
            sequenceToCall = c->params().value("sequenceName").toString();
            hasCallSequenceCommand = true;
        } else if (c->name() == "OPEN_SEQUENCE") {
            // Mark that we have an open sequence command and store the name
            openSequenceName = c->params().value("sequenceName").toString();
            hasOpenSequenceCommand = true;
        }
    }

    // If we have a CALL_SEQUENCE command, switch to that sequence
    if (hasCallSequenceCommand && !sequenceToCall.isEmpty()) {
        // First try to find the sequence in the main manager
        PromptSequence foundSeq;
        QString foundPath;
        const QMap<QString, PromptSequence>& allSequences = m_seqMgr.sequences();
        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            if (it.value().name() == sequenceToCall) {
                foundSeq = it.value();
                foundPath = it.key();

                // Check if summary is needed (analyze the params for a summary flag)
                Command* callSeqCommand = nullptr;
                for (Command* c : toRun) {
                    if (c->name() == "CALL_SEQUENCE") {
                        callSeqCommand = c;
                        break;
                    }
                }

                bool needSummary = true; // Default to true, could be controlled by a parameter

                if (callSeqCommand && callSeqCommand->params().contains("summarize") &&
                    callSeqCommand->params().value("summarize").isBool()) {
                    needSummary = callSeqCommand->params().value("summarize").toBool();
                }

                if (needSummary) {
                    // Summarize what was done in the current sequence before switching
                    QStringList completedStepsSummary;
                    for (int i = 0; i < m_currentStepIdx; ++i) {
                        if (i < m_currentSeq.steps().size()) {
                            QString stepDesc = m_currentSeq.steps()[i].tmpl().section('\n', 0, 0).trimmed();
                            if (stepDesc.length() > 80) stepDesc = stepDesc.left(80) + "...";
                            completedStepsSummary << QString("Шаг %1: %2").arg(i+1).arg(stepDesc);
                        }
                    }

                    // End the current command sequence before starting the new one
                    if (!m_currentSeqId.isEmpty() && m_contextManager) {
                        m_contextManager->endCommandSequence(m_currentSeqId);
                    }

                    m_currentSeq = foundSeq;
                    m_currentStepIdx = 0;
                    m_sequenceMode = true;

                    // Start a new command sequence for the new sequence
                    m_currentSeqId = m_contextManager->startCommandSequence(m_currentSeq.name(), m_currentSeq.description());

                    m_currentSeqPath = foundPath; // Update the sequence path

                    // Add context about what was done in the previous sequence
                    if (!completedStepsSummary.isEmpty()) {
                        QString summaryText = QString("Предыдущая последовательность выполнила: %1").arg(completedStepsSummary.join("; "));
                        m_history.push_back(qMakePair(QString("system"), summaryText));
                    }

                    addSequenceTransitionToHistory(sequenceToCall);  // Add transition to history
                    updateSequenceGraphHighlight();  // Update the graph highlighting
                    qDeleteAll(toRun);  // Clean up the commands
                    QTimer::singleShot(200, this, &MainWindow::runNextStep);
                    return;  // Exit early to switch to new sequence
                } else {
                    // No summary needed, directly switch to the new sequence
                    // End the current command sequence before starting the new one
                    if (!m_currentSeqId.isEmpty() && m_contextManager) {
                        m_contextManager->endCommandSequence(m_currentSeqId);
                    }

                    m_currentSeq = foundSeq;
                    m_currentStepIdx = 0;
                    m_sequenceMode = true;

                    // Start a new command sequence for the new sequence
                    m_currentSeqId = m_contextManager->startCommandSequence(m_currentSeq.name(), m_currentSeq.description());

                    m_currentSeqPath = foundPath; // Update the sequence path

                    addSequenceTransitionToHistory(sequenceToCall);  // Add transition to history
                    updateSequenceGraphHighlight();  // Update the graph highlighting
                    qDeleteAll(toRun);  // Clean up the commands
                    QTimer::singleShot(200, this, &MainWindow::runNextStep);
                    return;  // Exit early to switch to new sequence
                }
            }
        }

        // If not found in main manager, try to find in the template directory where the current sequence is located
        if (foundPath.isEmpty() && !m_currentSeqPath.isEmpty()) {
            QFileInfo currentSeqInfo(m_currentSeqPath);
            QDir templateDir = currentSeqInfo.absoluteDir();

            // Try multiple possible formats for the sequence filename
            QStringList possibleFilenames;

            // Add the exact name as provided
            possibleFilenames << sequenceToCall;

            // Add with .seq.json extension if not already present
            if (!sequenceToCall.endsWith(".seq.json")) {
                possibleFilenames << (sequenceToCall + ".seq.json");
            }

            // Add with just .json extension if not already present
            if (!sequenceToCall.endsWith(".json")) {
                possibleFilenames << (sequenceToCall + ".json");
            }

            // Add with just .seq extension if not already present
            if (!sequenceToCall.endsWith(".seq")) {
                possibleFilenames << (sequenceToCall + ".seq");
            }

            log("DEBUG: CALL_SEQUENCE - Searching for '" + sequenceToCall + "' in current sequence directory: " + templateDir.absolutePath());

            // Try each possible filename in the sequence directory
            for (const QString& filename : possibleFilenames) {
                QString sequenceFilePath = templateDir.filePath(filename);

                if (QFile::exists(sequenceFilePath)) {
                    // Load the sequence from this file
                    QFile seqFile(sequenceFilePath);
                    if (seqFile.open(QIODevice::ReadOnly)) {
                        QByteArray data = seqFile.readAll();
                        QJsonDocument doc = QJsonDocument::fromJson(data);
                        if (doc.isObject()) {
                            foundSeq = PromptSequence::fromJson(doc.object());
                            foundPath = sequenceFilePath;
                            log("DEBUG: CALL_SEQUENCE - Found sequence in current sequence directory: " + sequenceFilePath); // Debug log

                            // Check if summary is needed (analyze the params for a summary flag)
                            Command* callSeqCommand = nullptr;
                            for (Command* c : toRun) {
                                if (c->name() == "CALL_SEQUENCE") {
                                    callSeqCommand = c;
                                    break;
                                }
                            }

                            bool needSummary = true; // Default to true, could be controlled by a parameter

                            if (callSeqCommand && callSeqCommand->params().contains("summarize") &&
                                callSeqCommand->params().value("summarize").isBool()) {
                                needSummary = callSeqCommand->params().value("summarize").toBool();
                            }

                            if (needSummary) {
                                // Summarize what was done in the current sequence before switching
                                QStringList completedStepsSummary;
                                for (int i = 0; i < m_currentStepIdx; ++i) {
                                    if (i < m_currentSeq.steps().size()) {
                                        QString stepDesc = m_currentSeq.steps()[i].tmpl().section('\n', 0, 0).trimmed();
                                        if (stepDesc.length() > 80) stepDesc = stepDesc.left(80) + "...";
                                        completedStepsSummary << QString("Шаг %1: %2").arg(i+1).arg(stepDesc);
                                    }
                                }

                                // End the current command sequence before starting the new one
                                if (!m_currentSeqId.isEmpty() && m_contextManager) {
                                    m_contextManager->endCommandSequence(m_currentSeqId);
                                }

                                m_currentSeq = foundSeq;
                                m_currentStepIdx = 0;
                                m_sequenceMode = true;

                                // Start a new command sequence for the new sequence
                                m_currentSeqId = m_contextManager->startCommandSequence(m_currentSeq.name(), m_currentSeq.description());

                                m_currentSeqPath = foundPath; // Update the sequence path

                                // Add context about what was done in the previous sequence
                                if (!completedStepsSummary.isEmpty()) {
                                    QString summaryText = QString("Предыдущая последовательность выполнила: %1").arg(completedStepsSummary.join("; "));
                                    m_history.push_back(qMakePair(QString("system"), summaryText));
                                }

                                addSequenceTransitionToHistory(sequenceToCall);  // Add transition to history
                                updateSequenceGraphHighlight();  // Update the graph highlighting
                                qDeleteAll(toRun);  // Clean up the commands
                                QTimer::singleShot(200, this, &MainWindow::runNextStep);
                                return;  // Exit early to switch to new sequence
                            } else {
                                // No summary needed, directly switch to the new sequence
                                // End the current command sequence before starting the new one
                                if (!m_currentSeqId.isEmpty() && m_contextManager) {
                                    m_contextManager->endCommandSequence(m_currentSeqId);
                                }

                                m_currentSeq = foundSeq;
                                m_currentStepIdx = 0;
                                m_sequenceMode = true;

                                // Start a new command sequence for the new sequence
                                m_currentSeqId = m_contextManager->startCommandSequence(m_currentSeq.name(), m_currentSeq.description());

                                m_currentSeqPath = foundPath; // Update the sequence path

                                addSequenceTransitionToHistory(sequenceToCall);  // Add transition to history
                                updateSequenceGraphHighlight();  // Update the graph highlighting
                                qDeleteAll(toRun);  // Clean up the commands
                                QTimer::singleShot(200, this, &MainWindow::runNextStep);
                                return;  // Exit early to switch to new sequence
                            }
                        }
                        seqFile.close();
                    }
                }
            }
        }

        // If sequence not found, log error and continue with normal flow
        log("Указанная последовательность не найдена: " + sequenceToCall);
        // Add extensive debugging to identify what sequences are available
        const QMap<QString, PromptSequence>& allSequences_debug = m_seqMgr.sequences();
        log("DEBUG: CALL_SEQUENCE - Available sequences debugging:");
        log("  Total sequences in main manager: " + QString::number(allSequences_debug.size()));
        log("  Current sequence path: '" + m_currentSeqPath + "'");
        if (!m_currentSeqPath.isEmpty()) {
            QFileInfo currentSeqInfo(m_currentSeqPath);
            QDir templateDir = currentSeqInfo.absoluteDir();
            log("  Current sequence directory: " + templateDir.absolutePath());
            // List files in the current sequence directory
            QStringList filters = {"*.seq.json", "*.seq", "*.json"};
            QFileInfoList files = templateDir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
            log("  Sequence files in current directory (" + QString::number(files.size()) + "):");
            for (const QFileInfo& fileInfo : files) {
                log("    - File: " + fileInfo.fileName());
            }
        }
        log("  Searched sequence name: '" + sequenceToCall + "'");
        log("  Available sequence names and paths in main manager:");
        for (auto it = allSequences_debug.constBegin(); it != allSequences_debug.constEnd(); ++it) {
            QString seqName = it.value().name();
            QString filePath = it.key();
            QFileInfo fileInfo(filePath);
            QString fileName = fileInfo.fileName();
            QString baseName = fileInfo.baseName();
            log("    - Sequence name: '" + seqName + "'");
            log("    - File name: '" + fileName + "'");
            log("    - Base name: '" + baseName + "'");
            log("    - File path: " + filePath);
            log("    - Match details: name='" + seqName + "', filename='" + fileName + "', basename='" + baseName + "'");
        }
    }

    // If we have an OPEN_SEQUENCE command, switch to that sequence
    if (hasOpenSequenceCommand && !openSequenceName.isEmpty()) {
        // First try to find the sequence in the main manager
        PromptSequence foundSeq2;
        QString foundPath2;
        const QMap<QString, PromptSequence>& allSequences = m_seqMgr.sequences();
        for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
            if (it.value().name() == openSequenceName) {
                foundSeq2 = it.value();
                foundPath2 = it.key();

                // End the current command sequence before starting the new one
                if (!m_currentSeqId.isEmpty() && m_contextManager) {
                    m_contextManager->endCommandSequence(m_currentSeqId);
                }

                m_currentSeq = foundSeq2;
                m_currentStepIdx = 0;
                m_sequenceMode = true;

                // Start a new command sequence for the new sequence
                m_currentSeqId = m_contextManager->startCommandSequence(m_currentSeq.name(), m_currentSeq.description());

                m_currentSeqPath = foundPath2; // Update the sequence path

                addSequenceTransitionToHistory(openSequenceName);  // Add transition to history
                updateSequenceGraphHighlight();  // Update the graph highlighting
                qDeleteAll(toRun);  // Clean up the commands
                QTimer::singleShot(200, this, &MainWindow::runNextStep);
                return;  // Exit early to switch to new sequence
            }
        }

        // If not found in main manager, try to find in the template directory where the current sequence is located
        if (foundPath2.isEmpty() && !m_currentSeqPath.isEmpty()) {
            QFileInfo currentSeqInfo(m_currentSeqPath);
            QDir templateDir = currentSeqInfo.absoluteDir();

            // Try multiple possible formats for the sequence filename
            QStringList possibleFilenames;

            // Add the exact name as provided
            possibleFilenames << openSequenceName;

            // Add with .seq.json extension if not already present
            if (!openSequenceName.endsWith(".seq.json")) {
                possibleFilenames << (openSequenceName + ".seq.json");
            }

            // Add with just .json extension if not already present
            if (!openSequenceName.endsWith(".json")) {
                possibleFilenames << (openSequenceName + ".json");
            }

            // Add with just .seq extension if not already present
            if (!openSequenceName.endsWith(".seq")) {
                possibleFilenames << (openSequenceName + ".seq");
            }

            log("DEBUG: OPEN_SEQUENCE - Searching for '" + openSequenceName + "' in current sequence directory: " + templateDir.absolutePath());

            // Try each possible filename in the sequence directory
            for (const QString& filename : possibleFilenames) {
                QString sequenceFilePath = templateDir.filePath(filename);

                if (QFile::exists(sequenceFilePath)) {
                    // Load the sequence from this file
                    QFile seqFile(sequenceFilePath);
                    if (seqFile.open(QIODevice::ReadOnly)) {
                        QByteArray data = seqFile.readAll();
                        QJsonDocument doc = QJsonDocument::fromJson(data);
                        if (doc.isObject()) {
                            foundSeq2 = PromptSequence::fromJson(doc.object());
                            foundPath2 = sequenceFilePath;
                            log("DEBUG: OPEN_SEQUENCE - Found sequence in current sequence directory: " + sequenceFilePath); // Debug log

                            // End the current command sequence before starting the new one
                            if (!m_currentSeqId.isEmpty() && m_contextManager) {
                                m_contextManager->endCommandSequence(m_currentSeqId);
                            }

                            m_currentSeq = foundSeq2;
                            m_currentStepIdx = 0;
                            m_sequenceMode = true;

                            // Start a new command sequence for the new sequence
                            m_currentSeqId = m_contextManager->startCommandSequence(m_currentSeq.name(), m_currentSeq.description());

                            m_currentSeqPath = foundPath2; // Update the sequence path

                            addSequenceTransitionToHistory(openSequenceName);  // Add transition to history
                            updateSequenceGraphHighlight();  // Update the graph highlighting
                            qDeleteAll(toRun);  // Clean up the commands
                            QTimer::singleShot(200, this, &MainWindow::runNextStep);
                            return;  // Exit early to switch to new sequence
                        }
                        seqFile.close();
                    }
                }
            }
        }

        // If sequence not found, log error and continue with normal flow
        log("Указанная последовательность не найдена: " + openSequenceName);
        // Add extensive debugging to identify what sequences are available
        const QMap<QString, PromptSequence>& allSequences_debug2 = m_seqMgr.sequences();
        log("DEBUG: OPEN_SEQUENCE - Available sequences debugging:");
        log("  Total sequences in main manager: " + QString::number(allSequences_debug2.size()));
        log("  Current sequence path: '" + m_currentSeqPath + "'");
        if (!m_currentSeqPath.isEmpty()) {
            QFileInfo currentSeqInfo(m_currentSeqPath);
            QDir templateDir = currentSeqInfo.absoluteDir();
            log("  Current sequence directory: " + templateDir.absolutePath());
            // List files in the current sequence directory
            QStringList filters = {"*.seq.json", "*.seq", "*.json"};
            QFileInfoList files = templateDir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
            log("  Sequence files in current directory (" + QString::number(files.size()) + "):");
            for (const QFileInfo& fileInfo : files) {
                log("    - File: " + fileInfo.fileName());
            }
        }
        log("  Searched sequence name: '" + openSequenceName + "'");
        log("  Available sequence names and paths in main manager:");
        for (auto it = allSequences_debug2.constBegin(); it != allSequences_debug2.constEnd(); ++it) {
            QString seqName = it.value().name();
            QString filePath = it.key();
            QFileInfo fileInfo(filePath);
            QString fileName = fileInfo.fileName();
            QString baseName = fileInfo.baseName();
            log("    - Sequence name: '" + seqName + "'");
            log("    - File name: '" + fileName + "'");
            log("    - Base name: '" + baseName + "'");
            log("    - File path: " + filePath);
            log("    - Match details: name='" + seqName + "', filename='" + fileName + "', basename='" + baseName + "'");
        }
    }

    QStringList appliedSummary;
    for (Command* c : toRun) {
        QString one = c->name();
        QJsonObject p = c->params();
        if (p.contains("path"))
            one += QString(" path=%1").arg(p.value("path").toString());
        if (p.contains("from") && p.contains("to"))
            one += QString(" %1→%2").arg(p.value("from").toString(), p.value("to").toString());
        appliedSummary << one;
    }
    if (!appliedSummary.isEmpty()) {
        m_prevSteps << QString("Шаг %1: %2").arg(m_currentStepIdx + 1).arg(appliedSummary.join("; "));
    }

    QString err;
    auto logConsoleResult = [&](Command* cmd, const QString& errorMessage = QString()){
        // Use the new getResult method to get the command result
        QString result = cmd->getResult();

        if (!result.isEmpty()) {
            // Extract title from the first line - include the command and its parameter for uniqueness
            QString fullTitle = result.split('\n').first();  // Get the full first line like "RUN_SHELL: command" or "RUN_PYTHON: script"

            appendConsoleContext(fullTitle, result, QString(), errorMessage.isEmpty() ? (cmd->wasSuccessful() ? 0 : 1) : 1);

            // Also add to history so user can see command results in the chat
            m_history.push_back(qMakePair(QString("system"), result));
        }

        // Add command to context manager with result
        if (m_contextManager) {
            addCommandToContext(cmd, result);
        }
    };

    QList<Command*> pendingEdits;
    bool applyFailed = false;
    const QDateTime execStartedAt = QDateTime::currentDateTime();
    OllamaAdapter* execModel = &m_model;
    if (m_useAgentPipeline && ensureAgentModelConfigured())
        execModel = &m_agentModel;
    for (Command* c : toRun) {
        const QString name = c->name();
        if (name == "ASK_USER" || name == "OPEN_FILE" || name == "OPEN_FILE_REGION" || name == "SHOW_RESULT" || name == "REASONING"
            || name == "CONTEXT_SHOW" || name == "CONTEXT_CLEAR")
            continue;
        if (name == "EDIT_IN_RANGE") {
            pendingEdits << c;
            continue;
        }
        if (!c->applyWithLLM(&m_dev_manager, execModel, &err)) {
            c->setExecutionResult(false, err);  // Set execution result before getting output
            logConsoleResult(c, err);  // Pass the error message
            QMessageBox::warning(this, tr("Ошибка выполнения"), err);
            applyFailed = true;
            break;
        }
        c->setExecutionResult(true);  // Set execution result for successful execution
        handlePostApplyContextCleanup(c);
        logConsoleResult(c, QString());  // No error for successful execution
        handleSearchResults(c);
    }

    if (!applyFailed && !pendingEdits.isEmpty()) {
        if (!applyGroupedEditInRange(pendingEdits, &err)) {
            QMessageBox::warning(this, tr("Ошибка выполнения"), err);
        } else {
            for (Command* c : pendingEdits) {
                handlePostApplyContextCleanup(c);
                handleSearchResults(c);
            }
        }
    }

    rescan();

    if (applyFailed && !m_sequenceMode) {
        // Повторяем в не-последовательном режиме при ошибках
        retryCurrentSequenceStep();
    } else if (m_sequenceMode) {
        // Check if stop was requested before advancing to next step
        if (m_stopRequested) {
            m_stopRequested = false;
            m_sequenceMode = false;
            m_currentSeqId.clear(); // Clear the current sequence ID
            updateSequenceGraphHighlight();  // Update the graph highlighting (will clear highlight)

            // Show message that execution was stopped
            QMessageBox::information(this, "Остановлено",
                                     "Выполнение шаблона остановлено пользователем.");
            qDeleteAll(toRun);
            return;
        }

        const PromptStep &step = m_currentSeq.steps()[m_currentStepIdx];
        if (maybeAutoRepairAfterExecution(step, execStartedAt, toRun, applyFailed, err)) {
            qDeleteAll(toRun);
            return;
        }
        if (step.loop()) {
            // Если установлен флаг зацикливания, повторяем текущий шаг
            QTimer::singleShot(200, this, &MainWindow::runNextStep);
        } else {
            // В последовательности всегда переходим к следующему шагу (если не зацикливание)
            ++m_currentStepIdx;
            QTimer::singleShot(200, this, &MainWindow::runNextStep);
        }
    }

    qDeleteAll(toRun);
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
        setFullFileContext(normalized, allLines);
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
        addRegionContext(normalized, startLine, snippet);
        // Track that this file's content is in context from OPEN_FILE_REGION command
        m_fileToContextCommands[normalized].insert("OPEN_FILE_REGION");
        // Add file to watcher to monitor for external changes
        if (!m_fileWatcher->files().contains(normalized) && QFileInfo::exists(normalized)) {
            m_fileWatcher->addPath(normalized);
        }
        // Add command to context manager - include the actual numbered content
        if (m_contextManager) {
            QString numberedContent = formatNumberedLines(snippet, startLine);
            QString result = QString("OPEN_FILE_REGION: %1\n%2").arg(rawPath, numberedContent);
            addCommandToContext(cmd, result);
        }
    }
}

void MainWindow::rebuildContextCache()
{
    QStringList blocks;
    for (auto it = m_contextSlices.cbegin(); it != m_contextSlices.cend(); ++it) {
        const FileContextSlice& slice = it.value();
        if (slice.mode == FileContextSlice::None)
            continue;

        QString block = QString("==== %1 ====")
                .arg(displayContextPath(it.key(), m_projectRoot));
        if (slice.mode == FileContextSlice::Ranges)
            block += " (selected lines)";
        block += "\n";

        if (slice.mode == FileContextSlice::FullFile) {
            block += formatNumberedLines(slice.fullFileLines, 1);
        } else {
            for (const ContextRange& range : slice.regions) {
                // Include only the numbered lines without the range header for cleaner output
                block += formatNumberedLines(range.lines, range.startLine);
                block += "\n";
            }
            block = block.trimmed();
        }
        blocks << block.trimmed();
    }
    m_contextContent = blocks.join("\n\n");
    if (!m_consoleLogs.isEmpty()) {
        if (!m_contextContent.isEmpty())
            m_contextContent += "\n\n";
        m_contextContent += m_consoleLogs.join("\n\n");
    }
    if (!m_externalSources.isEmpty()) {
        if (!m_contextContent.isEmpty())
            m_contextContent += "\n\n";
        m_contextContent += m_externalSources.join("\n\n");
    }

    // If ContextManager is available, append its generated context
    if (m_contextManager) {
        QString contextManagerContent = m_contextManager->generateContextText();
        if (!contextManagerContent.isEmpty()) {
            if (!m_contextContent.isEmpty()) {
                m_contextContent += "\n\n";
            }
            m_contextContent += contextManagerContent;
        }
    }
}

QString MainWindow::formatNumberedLines(const QStringList& lines, int startLine) const
{
    if (lines.isEmpty())
        return QString();
    const int width = QString::number(startLine + lines.size() - 1).size();
    QStringList numbered;
    for (int i = 0; i < lines.size(); ++i) {
        const int lineNo = startLine + i;
        numbered << QString("%1| %2")
                        .arg(lineNo, width, 10, QChar('0'))
                        .arg(lines.at(i));
    }
    return numbered.join("\n");
}

void MainWindow::setFullFileContext(const QString& path, const QStringList& lines)
{
    const QString normalized = normalizeContextPath(path, m_projectRoot);
    if (normalized.isEmpty())
        return;
    FileContextSlice& slice = m_contextSlices[normalized];
    slice.mode = FileContextSlice::FullFile;
    slice.fullFileLines = lines;
    slice.regions.clear();
    rebuildContextCache();
}

void MainWindow::addRegionContext(const QString& path, int startLine, const QStringList& lines)
{
    const QString normalized = normalizeContextPath(path, m_projectRoot);
    if (normalized.isEmpty() || lines.isEmpty())
        return;

    FileContextSlice& slice = m_contextSlices[normalized];
    slice.mode = FileContextSlice::Ranges;
    slice.fullFileLines.clear();

    ContextRange incoming;
    incoming.startLine = startLine;
    incoming.lines = lines;
    incoming.endLine = startLine + lines.size() - 1;

    bool replaced = false;
    for (ContextRange& existing : slice.regions) {
        if (existing.startLine == incoming.startLine && existing.endLine == incoming.endLine) {
            existing.lines = lines;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        slice.regions.append(incoming);
        std::sort(slice.regions.begin(), slice.regions.end(),
                  [](const ContextRange& a, const ContextRange& b){
                      return a.startLine < b.startLine;
                  });
    }
    rebuildContextCache();
}

void MainWindow::clearContextForPath(const QString& path)
{
    const QString normalized = normalizeContextPath(path, m_projectRoot);
    if (normalized.isEmpty())
        return;
    if (m_contextSlices.remove(normalized) > 0) {
        rebuildContextCache();
    }
    // Remove from file watcher if it was being monitored
    if (m_fileWatcher->files().contains(normalized)) {
        m_fileWatcher->removePath(normalized);
    }
    // Remove from file-to-context-commands mapping
    m_fileToContextCommands.remove(normalized);
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
        clearContextForPath(it.key());
    }
    return true;
}

void MainWindow::handlePostApplyContextCleanup(Command* cmd)
{
    const QString name = cmd->name();
    const QJsonObject p = cmd->params();
    auto clear = [&](const QString& key) {
        if (!key.isEmpty())
            clearContextForPath(key);
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
        const auto keys = m_contextSlices.keys();
        for (const QString& k : keys) {
            clearContextForPath(k);
        }
        rebuildContextCache();
        return;
    }

    if (name == "UNDO_LAST_CHANGE") {
        // We don't know which files changed; clear all file-context slices.
        const auto keys = m_contextSlices.keys();
        for (const QString& k : keys) {
            clearContextForPath(k);
        }
        rebuildContextCache();
        return;
    }

    // Handle RunShellCommand - remove old output when command is run again
    if (name == "RUN_SHELL") {
        QString command = p.value("command").toString();
        if (!command.isEmpty()) {
            // Remove existing output for this shell command
            auto it = std::remove_if(m_consoleLogs.begin(), m_consoleLogs.end(),
                [&command](const QString& log) {
                    return log.startsWith("==== RUN_SHELL: " + command);
                });
            m_consoleLogs.erase(it, m_consoleLogs.end());
            m_shellCommandOutputs.remove(command);
            rebuildContextCache();
        }
        return;
    }

    // Handle RunPythonCommand - remove old output when same script is run again
    if (name == "RUN_PYTHON") {
        QString scriptPath = p.value("scriptPath").toString();
        QString code = p.value("code").toString();
        QString identifier = !scriptPath.isEmpty() ? scriptPath : "inline code";

        if (!identifier.isEmpty()) {
            // Remove existing output for this Python script/command
            auto it = std::remove_if(m_consoleLogs.begin(), m_consoleLogs.end(),
                [&identifier](const QString& log) {
                    return log.startsWith("==== RUN_PYTHON: " + identifier);
                });
            m_consoleLogs.erase(it, m_consoleLogs.end());
            m_pythonScriptOutputs.remove(identifier);
            rebuildContextCache();
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
    return m_externalSources.join("\n\n");
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
        // Remove existing output for this command
        auto it = std::remove_if(m_consoleLogs.begin(), m_consoleLogs.end(),
            [&cmd](const QString& log) {
                return log.startsWith("==== RUN_SHELL: " + cmd);
            });
        m_consoleLogs.erase(it, m_consoleLogs.end());
        m_shellCommandOutputs.remove(cmd);
    }
    // If this is a RUN_PYTHON command output, replace existing output for the same script
    else if (title.startsWith("RUN_PYTHON: ")) {
        QString script = title.mid(12); // Extract the script part after "RUN_PYTHON: "
        // Remove existing output for this script
        auto it = std::remove_if(m_consoleLogs.begin(), m_consoleLogs.end(),
            [&script](const QString& log) {
                return log.startsWith("==== RUN_PYTHON: " + script);
            });
        m_consoleLogs.erase(it, m_consoleLogs.end());
        m_pythonScriptOutputs.remove(script);
    }

    m_consoleLogs << block.trimmed();
    const int maxLogs = 5;
    while (m_consoleLogs.size() > maxLogs)
        m_consoleLogs.removeFirst();
    rebuildContextCache();
}

void MainWindow::appendExternalContext(const QString& block)
{
    if (block.trimmed().isEmpty())
        return;
    m_externalSources << block.trimmed();
    const int maxBlocks = 5;
    while (m_externalSources.size() > maxBlocks)
        m_externalSources.removeFirst();
    rebuildContextCache();
}

void MainWindow::retryCurrentSequenceStep()
{
    if (!m_sequenceMode)
        return;

    // Check if stop was requested
    if (m_stopRequested) {
        m_stopRequested = false;
        m_sequenceMode = false;
        m_currentSeqId.clear(); // Clear the current sequence ID
        updateSequenceGraphHighlight();  // Update the graph highlighting (will clear highlight)

        // Show message that execution was stopped
        QMessageBox::information(this, "Остановлено",
                                 "Выполнение шаблона остановлено пользователем.");
        return;
    }

    QTimer::singleShot(200, this, &MainWindow::runNextStep);
}

bool MainWindow::ensureAgentModelConfigured()
{
    if (!m_useAgentPipeline)
        return false;

    if (m_agentModel.selectedModel().isEmpty()) {
        // Default agent model. If not installed, we'll fall back to the currently selected model.
        m_agentModel.configure(QStringLiteral("gpt-oss:20b"), QVariantMap());
    }

    QString err;
    if (!m_agentModel.isAvailable(&err)) {
        log(QString("Agent model unavailable (%1). Falling back to selected model.").arg(err));
        return false;
    }
    return true;
}

QStringList MainWindow::buildAllowedCommandsDocsCompact(const PromptStep& step) const
{
    QStringList out;
    const QSet<QString> allowed = QSet<QString>::fromList(step.allowedCommands());
    for (Command* c : CommandRegistry::all()) {
        const QString name = c->name();
        if (!allowed.contains(name)) {
            delete c;
            continue;
        }
        const CommandDoc doc = CommandDocs::doc(name);
        out << CommandDocs::renderForLLM(doc, true);
        delete c;
    }
    return out;
}

QByteArray MainWindow::repairToJsonArrayIfNeeded(const QByteArray& rawText, const PromptStep& step)
{
    const QByteArray extracted = extractJsonPart(rawText);
    if (!extracted.isEmpty())
        return extracted;

    if (!ensureAgentModelConfigured())
        return QByteArray();

    const QStringList allowedDocs = buildAllowedCommandsDocsCompact(step);
    const QString systemPrompt =
        QStringLiteral(
            "You are a strict JSON repair tool.\n"
            "Task: Convert the INPUT into a valid JSON array of commands.\n"
            "Rules:\n"
            "- Output ONLY a JSON array. No markdown, no comments.\n"
            "- Each element is an object with keys: command, params.\n"
            "- params MUST be a JSON object (not a string).\n"
            "- Only use commands from the allowed list below.\n"
            "- If there is not enough info to act, output exactly one ASK_USER command.\n"
            "- If no actions are needed, output [].\n\n"
            "Allowed commands:\n%1\n")
            .arg(allowedDocs.join(QStringLiteral("\n")));

    QVariantMap opts;
    opts.insert("temperature", 0.05);
    opts.insert("num_predict", 800);
    opts.insert("timeoutMs", 45000);

    const QString repaired = m_agentModel.generateWithSystem(QString::fromUtf8(rawText), systemPrompt, opts);
    const QByteArray repairedUtf8 = repaired.toUtf8();
    const QByteArray repairedJson = extractJsonPart(repairedUtf8);
    return repairedJson.isEmpty() ? QByteArray() : repairedJson;
}

bool MainWindow::isImplementationIntentText(const QString& text)
{
    const QString t = text.toLower();
    static const QStringList needles = {
        // RU
        "реализ", "сделай", "создай", "добав", "исправ", "почин", "внедр", "напис",
        "сгенер", "передел", "улучш", "доработ", "перепиш",
        // EN
        "implement", "create", "add", "fix", "repair", "refactor", "rewrite", "build", "generate"
    };
    for (const QString& n : needles) {
        if (t.contains(n))
            return true;
    }
    return false;
}

static QString normalizePathForProject(const QString& raw, const QString& projectRoot)
{
    if (raw.trimmed().isEmpty())
        return QString();
    QFileInfo info(raw);
    if (info.isAbsolute())
        return QDir::cleanPath(info.absoluteFilePath());
    return QDir::cleanPath(QDir(projectRoot).absoluteFilePath(raw));
}

bool MainWindow::didAnyAffectedPathChangeSince(const QDateTime& since, const QList<Command*>& executedCommands) const
{
    if (!since.isValid())
        return false;

    static const QStringList keys = {
        "path",
        "oldPath",
        "newPath",
        "folderPath",
        "scriptPath",
        "sourceFilePath"
    };

    QSet<QString> touched;
    for (const Command* c : executedCommands) {
        if (!c)
            continue;
        const QJsonObject params = c->params();
        for (const QString& k : keys) {
            const QString raw = params.value(k).toString();
            const QString p = normalizePathForProject(raw, m_projectRoot);
            if (!p.isEmpty())
                touched.insert(p);
        }
    }

    for (const QString& p : touched) {
        QFileInfo fi(p);
        if (fi.exists() && fi.lastModified() >= since)
            return true;
    }
    return false;
}

bool MainWindow::maybeAutoRepairAfterExecution(const PromptStep& step,
                                              const QDateTime& execStartedAt,
                                              const QList<Command*>& executedCommands,
                                              bool applyFailed,
                                              const QString& lastError)
{
    if (!m_sequenceMode)
        return false;
    if (!m_useAgentPipeline)
        return false;
    if (m_inAgentAutoRepair)
        return false;

    // Reset per-step counter when step/sequence changes.
    if (m_agentRepairStepIdx != m_currentStepIdx || m_agentRepairSeqId != m_currentSeqId) {
        m_agentRepairCountThisStep = 0;
        m_agentRepairStepIdx = m_currentStepIdx;
        m_agentRepairSeqId = m_currentSeqId;
    }

    if (m_agentRepairCountThisStep >= m_agentMaxRepairsPerStep)
        return false;

    const bool implIntent = isImplementationIntentText(m_lastUserMessage);
    const bool changed = didAnyAffectedPathChangeSince(execStartedAt, executedCommands);
    const bool needRepair = applyFailed || (implIntent && !changed);
    if (!needRepair)
        return false;

    if (!ensureAgentModelConfigured())
        return false;

    const QStringList allowedDocs = buildAllowedCommandsDocsCompact(step);
    const QString ctx = m_contextManager ? m_contextManager->generateContextText() : QString();
    const QString systemPrompt =
        QStringLiteral(
            "You are an expert developer agent that must output ONLY a JSON array of commands.\n"
            "Goal: make real progress towards the USER REQUEST using the allowed commands.\n"
            "Rules:\n"
            "- Output ONLY JSON array, no extra text.\n"
            "- Use as few commands as possible (1-5).\n"
            "- Prefer file-editing commands when implementation is requested.\n"
            "- If missing critical info, output exactly one ASK_USER.\n"
            "- Do NOT loop on CALL_SEQUENCE.\n\n"
            "Allowed commands:\n%1\n")
            .arg(allowedDocs.join(QStringLiteral("\n")));

    QString userPrompt = QStringLiteral("USER REQUEST:\n%1\n").arg(m_lastUserMessage);
    if (applyFailed && !lastError.trimmed().isEmpty()) {
        userPrompt += QStringLiteral("\nLAST ERROR:\n%1\n").arg(lastError);
    }
    if (!ctx.trimmed().isEmpty()) {
        userPrompt += QStringLiteral("\nRECENT CONTEXT (executed commands/results):\n%1\n").arg(ctx.left(12000));
    }

    QVariantMap opts;
    opts.insert("temperature", 0.1);
    opts.insert("num_predict", 1200);
    opts.insert("timeoutMs", 60000);

    const QString repair = m_agentModel.generateWithSystem(userPrompt, systemPrompt, opts);
    const QByteArray repairedJson = extractJsonPart(repair.toUtf8());
    if (repairedJson.isEmpty())
        return false;

    m_agentRepairCountThisStep++;
    m_inAgentAutoRepair = true;
    appendConsoleContext(QStringLiteral("AGENT_REPAIR_DRAFT"), repair, QString(), 0);

    QTimer::singleShot(0, this, [this, repairedJson]() {
        m_streamBuf = QString::fromUtf8(repairedJson);
        handleAssistantFinished();
        m_inAgentAutoRepair = false;
    });
    return true;
}

void MainWindow::runNextSequence()
{
    // This method is called when transitioning to a new sequence
    if (!m_sequenceMode) return;
    if (m_currentStepIdx < 0 ||
            m_currentStepIdx >= m_currentSeq.steps().size()) {
        // конец последовательности
        QMessageBox::information(this, "Готово",
                                 "Последовательность выполнена полностью.");
        // End the command sequence if we were tracking one
        if (!m_currentSeqId.isEmpty() && m_contextManager) {
            m_contextManager->endCommandSequence(m_currentSeqId);
        }
        m_sequenceMode = false;
        m_currentSeqId.clear(); // Clear the current sequence ID
        stopTaskTimer(); // Stop the task timer when sequence completes
        updateSequenceGraphHighlight();  // Update the graph highlighting (will clear highlight)
        return;
    }

    const PromptStep &step = m_currentSeq.steps()[m_currentStepIdx];
    PromptResolver::ResolvedPrompt resolved = PromptResolver::resolve(step.tmpl(), step, this);
    qDebug() << "[runNextSequence] systemPrompt snippet:" << resolved.systemPrompt.left(200);
    qDebug() << "[runNextSequence] userPrompt:" << resolved.userPrompt;

    // Оценка токенов для контекста шага
    {
        int tokens = m_model.estimateTokens(resolved.systemPrompt + "\n" + resolved.userPrompt);
        log(QString("[Шаг %1] Контекст ≈ %2 токенов").arg(m_currentStepIdx+1).arg(tokens));
    }
    // Логируем, показываем пользователю
    log(QString("\n--- Шаг %1 ------------------------------------------------")
        .arg(m_currentStepIdx+1));
    log(resolved.systemPrompt);
    if (!resolved.userPrompt.isEmpty())
        log(QString("USER_QUERY:\n%1").arg(resolved.userPrompt));

    // Отправляем запрос так же, как обычный ввод пользователя
    m_streamBuf.clear();

    // Добавляем отладочный вывод, чтобы понять, что отправляем в LLM
    qDebug() << "[DEBUG SEND TO LLM] - USER PROMPT:" << resolved.userPrompt;
    qDebug() << "[DEBUG SEND TO LLM] - SYSTEM PROMPT (first 500 chars):" << resolved.systemPrompt.left(500);
    qDebug() << "[DEBUG SEND TO LLM] - SYSTEM PROMPT LENGTH:" << resolved.systemPrompt.length();

    m_currentPrompt = resolved.systemPrompt + "\n" + resolved.userPrompt;  // Store the current prompt

    if (m_useAgentPipeline && ensureAgentModelConfigured()) {
        QVariantMap compileOpts;
        compileOpts.insert("temperature", 0.1);
        compileOpts.insert("num_predict", 1400);
        compileOpts.insert("timeoutMs", 90000);

        const QString compileSystem = resolved.systemPrompt + QStringLiteral("\n\nIMPORTANT: Output ONLY a JSON array of commands. No extra text.\n");
        const QString compiled = m_agentModel.generateWithSystem(resolved.userPrompt, compileSystem, compileOpts);
        QTimer::singleShot(0, this, [this, compiled]() {
            m_streamBuf = compiled;
            handleAssistantFinished();
        });
        return;
    }

    if (!m_activeImagesBase64.isEmpty()) {
        m_model.startStreamWithSystemAndImages(resolved.userPrompt, resolved.systemPrompt, m_activeImagesBase64, QVariantMap());
    } else {
        m_model.startStreamWithSystem(resolved.userPrompt, resolved.systemPrompt);
    }
}

void MainWindow::addSequenceTransitionToHistory(const QString& sequenceName)
{
    // Add a transition message to the history when switching sequences
    QString transitionMsg = QString("Переход к последовательности: %1").arg(sequenceName);
    m_history.push_back(qMakePair(QString("system"), transitionMsg));
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
    // Путь к файлу default.seq.json
    QString defaultFile = QDir(m_seqMgr.root()).filePath("default.seq.json");

    if (QFile::exists(defaultFile))
        return;        // уже есть – ничего не делаем

    // Создаём объект‑шаблон
    PromptSequence seq = defaultSequenceTemplate();

    // Сохраняем
    if (!m_seqMgr.save(seq, defaultFile))
        qWarning() << "Не удалось создать файл default.seq.json";
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


/* ────────────────────────────────────────────────────── */
PromptSequence MainWindow::defaultSequenceTemplate()
{
    // Step 1: Clarify the task if needed
    PromptStep s1;
    s1.setTemplate(
        "Ты — ИИ-помощник по рефакторингу и правкам кода.\n"
        "Контекст проекта:\n{{PROJECT_ROOT}}"
        "Вот список файлов:\n{{FILE_LIST}}\n"
        "\n{{HISTORY}}\n"
        "Если задача пользователя не до конца ясна — задай до 3 уточняющих вопросов, используя команду ASK_USER (один JSON на вопрос)."
        "При необходимости можешь сразу вызывать SEARCH_PROJECT (поиск по проекту) или SEARCH_WEB (внешние источники), прежде чем формировать план."
        "Доступные команды: {{SUPPORTED_COMMANDS}}"
        "Затем опиши краткий план действий."
    );
    s1.setAllowedCommands(QStringList{ "ASK_USER", "OPEN_FILE_REGION", "OPEN_FILE", "SEARCH_PROJECT", "SEARCH_WEB" });

    // Step 2: Gather minimal relevant context (snippets only)
    PromptStep s2;
    s2.setTemplate(
        "На основе запроса и плана выбери минимальные необходимые участки кода."
        "Используй OPEN_FILE_REGION (желательно c startLine/endLine или регуляркой) для точечных фрагментов."
        "Если нужен целый файл (редко) — используй OPEN_FILE."
        "Для полнотекстового поиска по проекту используй SEARCH_PROJECT (укажи maxMatches и, при необходимости, include)."
        "Если требуются внешние сведения — вызови SEARCH_WEB и кратко перескажи найденные материалы."
        "Выведи только команды."
    );
    s2.setAllowedCommands(QStringList{ "OPEN_FILE_REGION", "OPEN_FILE", "SEARCH_PROJECT", "SEARCH_WEB" });

    // Step 3: Propose concrete edits as commands (dry‑run)
    PromptStep s3;
    s3.setTemplate(
        "Сформируй набор правок."
        "• Для замены — REPLACE_IN_FILE"
        "• Для правки диапазона — EDIT_IN_RANGE"
        "• Для создания — CREATE_FILE"
        "• Для переименования/удаления — RENAME_FILE/DELETE_FILE"
        "Покажи только список JSON-команд без пояснений.\n"
        "Контекст:\n{{OPEN_FILES}}\n"
    );
    s3.setAllowedCommands(QStringList{ "REPLACE_IN_FILE", "EDIT_IN_RANGE", "CREATE_FILE", "RENAME_FILE", "DELETE_FILE" });

    // Step 4: If compilation hints are needed, ask short questions (optional)
    PromptStep s4;
    s4.setTemplate(
        "Если для точности правок не хватает 1-2 деталей — задай вопросы через ASK_USER, иначе верни пустой список."
    );
    s4.setAllowedCommands(QStringList{ "ASK_USER" });

    // Step 5: Finalize edits
    PromptStep s5;
    s5.setTemplate(
        "Выведи окончательные команды для применения изменений (повтори весь набор без пояснений)."
        "Если нужно показать пользователю результат или инструкцию — добавь команду SHOW_RESULT с title/content."
    );
    s5.setAllowedCommands(QStringList{ "REPLACE_IN_FILE", "EDIT_IN_RANGE", "CREATE_FILE", "RENAME_FILE", "DELETE_FILE", "SHOW_RESULT" });

    PromptSequence seq("Default");
    seq.setDescription("Поэтапная последовательность правок: уточнение → сбор минимального контекста → черновик правок → уточнения → финальные команды.");
    seq.addStep(s1);
    seq.addStep(s2);
    seq.addStep(s3);
    seq.addStep(s4);
    seq.addStep(s5);
    return seq;
};







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
    // Ask user for name
    bool ok=false;
    QString name = QInputDialog::getText(this, "New sequence", "Name:", QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    // Ask user whether to create in PromptSequences folder or in a template folder
    QMessageBox msgBox;
    msgBox.setWindowTitle("Create Sequence");
    msgBox.setText("Where would you like to create the sequence?");
    QAbstractButton* promptSeqButton = msgBox.addButton("In PromptSequences folder", QMessageBox::ActionRole);
    QAbstractButton* templateButton = msgBox.addButton("In Template folder", QMessageBox::ActionRole);
    QAbstractButton* cancelButton = msgBox.addButton(QMessageBox::Cancel);
    msgBox.exec();

    if (msgBox.clickedButton() == cancelButton) {
        return;
    } else if (msgBox.clickedButton() == promptSeqButton) {
        // Determine directory to save in PromptSequences
        QString dir;
        if (!m_seqMgr.fileList().isEmpty()) {
            dir = QFileInfo(m_seqMgr.fileList().first()).absolutePath();
        } else {
            dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                    .append("/DevelopersAssistant/PromptSequences");
            QDir().mkpath(dir);
            m_seqMgr.setRoot(dir);
        }
        QString filePath = QDir(dir).filePath(name.trimmed() + ".seq.json");

        // Create default template and save
        PromptSequence seq = defaultSequenceTemplate();
        seq.setName(name.trimmed());
        if (!m_seqMgr.save(seq, filePath)) {
            QMessageBox::warning(this, "Error", "Cannot save sequence to: " + filePath);
            return;
        }
        refreshAllSequences();
        ensureDefaultSequence();
        QMessageBox::information(this, "OK", "Sequence created:\n" + filePath);
    } else if (msgBox.clickedButton() == templateButton) {
        // Create in template folder - user needs to select a template directory
        QString templateDir = QFileDialog::getExistingDirectory(this, "Select Template Folder",
                                                              QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        if (templateDir.isEmpty()) return;

        // Verify that the selected directory is a valid template directory by checking for template.templ.json
        QString templateJsonPath = QDir(templateDir).filePath("template.templ.json");
        if (!QFile::exists(templateJsonPath)) {
            QMessageBox::warning(this, "Invalid Template Directory",
                               "The selected directory is not a valid template directory (missing template.templ.json).");
            return;
        }

        // Use the SolutionTemplateManager to create the new sequence in the template folder
        // This will create a default sequence structure in the template directory
        if (!m_templateMgr.createNewSequenceInTemplate(templateDir, name.trimmed())) {
            QMessageBox::warning(this, "Error", "Cannot create sequence in template directory: " + templateDir);
            return;
        }

        // Now we need to refresh both sequence and template managers to recognize the new sequence
        m_templateMgr.refresh();
        refreshAllSequences(); // This will refresh both the main sequences and those in templates
        QMessageBox::information(this, "OK", "Sequence created in template:\n" + templateDir + "/" + name.trimmed() + ".seq.json");
    }
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
    qDebug() << "[refreshAllSequences] Starting refresh of all sequences";

    // First, refresh the main sequence manager to get base sequences
    m_seqMgr.refresh();
    QStringList allSequencePaths = m_seqMgr.fileList();
    qDebug() << "[refreshAllSequences] Main sequence manager refreshed, base file count:" << allSequencePaths.size();

    // Then scan template folders for additional sequences and load them into the sequence manager
    const QStringList& templateDirs = m_templateMgr.templateDirs();
    qDebug() << "[refreshAllSequences] Processing" << templateDirs.size() << "template directories";

    for (const QString& templateDir : templateDirs) {
        QDir dir(templateDir);
        if (dir.exists()) {
            // ищем *.seq.json в папке шаблона
            QStringList filters = {"*.seq.json"};
            QFileInfoList files = dir.entryInfoList(filters,
                                                    QDir::Files | QDir::NoSymLinks,
                                                    QDir::Name);
            qDebug() << "[refreshAllSequences] Found" << files.size() << "sequence files in" << templateDir;

            for (const QFileInfo& fileInfo : files) {
                QString path = fileInfo.absoluteFilePath();

                // Check if this sequence is already loaded to avoid duplicates
                if (!allSequencePaths.contains(path)) {
                    // Load the sequence from the file and add it to the sequence manager
                    QFile seqFile(path);
                    if (seqFile.open(QIODevice::ReadOnly)) {
                        QByteArray data = seqFile.readAll();
                        QJsonDocument doc = QJsonDocument::fromJson(data);
                        if (doc.isObject()) {
                            PromptSequence seq = PromptSequence::fromJson(doc.object());
                            // Add to sequence manager cache using the addSequenceToCache method
                            m_seqMgr.addSequenceToCache(path, seq);
                            qDebug() << "[refreshAllSequences] Loaded and added sequence from template:" << path;
                        }
                        seqFile.close();
                    }
                } else {
                    qDebug() << "[refreshAllSequences] Sequence already in manager, skipping:" << path;
                }
            }
        }
    }

    // Update the allSequencePaths list to reflect the current state after adding template sequences
    allSequencePaths = m_seqMgr.fileList();
    qDebug() << "[refreshAllSequences] Completed discovery and loading, total sequence files in manager:" << allSequencePaths.size();

    // Add comprehensive logging about all available sequences for debugging
    const QMap<QString, PromptSequence>& allSequences = m_seqMgr.sequences();
    qDebug() << "[refreshAllSequences] Detailed sequence list for debugging:";
    log("=== SEQUENCE REFRESH DEBUG INFO ===");
    log("Total sequences discovered: " + QString::number(allSequences.size()));
    for (auto it = allSequences.constBegin(); it != allSequences.constEnd(); ++it) {
        QString seqName = it.value().name();
        QString filePath = it.key();
        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName();
        log("  - Sequence: '" + seqName + "' -> File: '" + fileName + "' at " + filePath);
    }
    log("=== END SEQUENCE DEBUG INFO ===");
}

void MainWindow::updateContextStatusDisplay()
{
    if (!m_contextCommandsList) {
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
            clearContextForPath(path);

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
    startTaskTimer(); // Start the task timer when template begins
    runNextStep();  // Start the sequence immediately
}


void MainWindow::runSimpleChat(const QString &text)
{
    // This method is called when "Простой чат" is selected - direct chat mode
    // Send the message directly to the model in streaming mode without templates/sequences

    qDebug() << "[runSimpleChat] Text:" << text;

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
    if (!m_resultDisplayWidget)
        return;

    const QString trimmedQuestion = question.trimmed();
    if (trimmedQuestion.isEmpty())
        return;

    m_lastQuestionTimestamp = QDateTime::currentDateTime();

    const QString formattedQuestion = QString("\n\n%1\n").arg(trimmedQuestion);
    m_resultDisplayWidget->appendResult(tr("Вопрос пользователя"),
                                        formattedQuestion,
                                        ResultDisplayWidget::EntryType::UserQuestion);
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
    // Set the stop flag to true to indicate that template execution should be stopped
    m_stopRequested = true;

    // If we're in sequence mode, the next runNextStep call will check the flag and stop
    if (m_sequenceMode) {
        log("Шаблонное выполнение остановлено пользователем.");
        stopTaskTimer(); // Stop the task timer when user stops execution
    }
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

void MainWindow::createStatusWidget()
{
    // Create a status widget to display the task information
    m_statusWidget = new QLabel(this);
    m_statusWidget->setStyleSheet("QLabel { background-color: #f0f0f0; border: 1px solid gray; padding: 2px; }");
    m_statusWidget->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statusWidget->setMinimumWidth(300); // Set minimum width for the status widget
    m_statusWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // Add the status widget to the main window's status bar
    statusBar()->addPermanentWidget(m_statusWidget);
}

void MainWindow::updateStatusDisplay()
{
    // Calculate and format the elapsed time
    QString timeStr;
    if (m_timerActive) {
        // Calculate total elapsed time: accumulated time + current elapsed time
        int totalElapsedMs = m_totalElapsedTime;
        if (m_taskRunning) {
            // Add the currently running time to the accumulated time
            totalElapsedMs += m_elapsedTime.elapsed();
        }
        QTime elapsedTime = QTime(0, 0).addMSecs(totalElapsedMs);
        timeStr = elapsedTime.toString("hh:mm:ss");
        qDebug() << "[DEBUG] Timer active - Total elapsed time (ms):" << totalElapsedMs << "Time string:" << timeStr;
    } else if (m_totalElapsedTime > 0) {
        // Timer is not active, but we have accumulated time from last run, show it
        QTime elapsedTime = QTime(0, 0).addMSecs(m_totalElapsedTime);
        timeStr = elapsedTime.toString("hh:mm:ss");
        //qDebug() << "[DEBUG] Timer not active but showing accumulated time (ms):" << m_totalElapsedTime << "Time string:" << timeStr;
    } else {
        timeStr = "00:00:00";
        //qDebug() << "[DEBUG] Timer not active - Showing default time";
    }

    // Get context usage using estimateTokens for both used and total
    quint64 usedTokens = 0;
    quint64 totalTokens = 0;

    // Calculate context usage based on the current prompt being sent to the model
    if (!m_currentPrompt.isEmpty()) {
        // Use the actual prompt that's being sent to the model
        usedTokens = m_model.estimateTokens(m_currentPrompt);
    } else {
        // Fallback: calculate based on the context content
        if (m_contextManager) {
            QString contextText = m_contextManager->generateContextText();
            usedTokens = m_model.estimateTokens(contextText);
        } else {
            usedTokens = m_model.estimateTokens(m_contextContent);
        }

        // Add currently streamed content if any
        if (!m_streamBuf.isEmpty()) {
            usedTokens += m_model.estimateTokens(m_streamBuf);
        }
    }

    // Get total context window size from Ollama adapter
    totalTokens = m_model.generationParams().num_ctx;

    // Make sure usedTokens doesn't exceed totalTokens
    if (usedTokens > totalTokens) {
        usedTokens = totalTokens; // Cap at maximum
    }

    // Format the token counts with abbreviations
    QString usedTokenStr = formatNumber(usedTokens);
    QString totalTokenStr = formatNumber(totalTokens);
    double pers = 100 - (double)usedTokens/totalTokens*100;
    QString usedTokenStrPers = QString::number(pers,'f', 2);

    // Update the status widget text
    QString statusText = QString("Time: %1 | Context left: %2/%3 (%4%)")
                            .arg(timeStr)
                            .arg(usedTokenStr)
                            .arg(totalTokenStr)
                            .arg(usedTokenStrPers);

    if (m_statusWidget) {
        m_statusWidget->setText(statusText);
    }
}

void MainWindow::startTaskTimer()
{
    qDebug() << "[DEBUG] startTaskTimer called. m_taskRunning:" << m_taskRunning << "m_timerActive:" << m_timerActive;
    if (!m_taskRunning) {
        // First time starting the task - initialize everything
        m_elapsedTime.start();  // Start the timer
        m_taskRunning = true;
        m_timerActive = true;
        m_totalElapsedTime = 0; // Reset for a new template execution session
        qDebug() << "[DEBUG] Timer started. Current time reset for new session.";
    } else {
        // Task is already running (e.g. continuing execution), ensure timer is active
        m_timerActive = true;
        qDebug() << "[DEBUG] Task already running, ensuring timer is active.";
    }
    updateStatusDisplay();
}

void MainWindow::stopTaskTimer()
{
    qDebug() << "[DEBUG] stopTaskTimer called. m_timerActive:" << m_timerActive << "m_taskRunning:" << m_taskRunning;
    if (m_taskRunning) {
        // Add the elapsed time to the total when stopping
        int currentElapsed = m_elapsedTime.elapsed();
        m_totalElapsedTime += currentElapsed;
        qDebug() << "[DEBUG] Adding elapsed time to total:" << currentElapsed << "New total:" << m_totalElapsedTime;
    } else {
        qDebug() << "[DEBUG] Task not running, not adding elapsed time";
    }
    m_timerActive = false;
    m_taskRunning = false;  // Reset the running flag
    updateStatusDisplay();
}

QString MainWindow::formatNumber(quint64 num) const
{
    if (num >= 1000000) { // 1M+
        double value = static_cast<double>(num) / 1000000.0;
        // Only show 2 decimal places if needed, otherwise show whole number
        if (value == static_cast<int>(value)) {
            return QString::number(static_cast<int>(value)) + "млн";
        } else {
            return QString::number(value, 'f', 2) + "млн";
        }
    } else if (num >= 1000) { // 1k+
        double value = static_cast<double>(num) / 1000.0;
        // Only show 2 decimal places if needed, otherwise show whole number
        if (value == static_cast<int>(value)) {
            return QString::number(static_cast<int>(value)) + "к";
        } else {
            return QString::number(value, 'f', 2) + "к";
        }
    } else {
        return QString::number(num);
    }
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

