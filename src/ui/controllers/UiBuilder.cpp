#include "UiBuilder.h"
#include <QObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QFrame>
#include <QTabWidget>
#include <QTreeWidget>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include "../../audio/SpeechRecognizer.h"
#include "MemoryLineEdit.h"
#include "StyledButton.h"
#include "MemoryComboBox.h"
#include "MemorySplitter.h"
#include "StyleTextEdit.h"
#include "IDEWidget.h"
#include "ChatDialog.h"
#include "GraphWidget.h"
#include "SequenceGraphWidget.h"
#include "ContextGraphWidget.h"
#include "ResultDisplayWidget.h"
#include "OllamaSettingsDialog.h"
#include "PromptEditorDialog.h"
#include "SolutionTemplateViewer.h"
#include "RunTemplateOrSequenceDialog.h"
#include "TtsController.h"
#include "../../adapters/OllamaAdapter.h"
#include "../../core/PromptSequenceManager.h"
#include "../../core/SolutionTemplateManager.h"

void UiBuilder::build(UIWidgets& ui,
                      std::function<QString(const char*)> tr_fn,
                      OllamaAdapter* model,
                      PromptSequenceManager* seqMgr,
                      SolutionTemplateManager* tmplMgr,
                      TtsController* tts,
                      QWidget* parent)
{
    // IDE widget
    ui.ide_widget = new IDEWidget("ideSplitter", Qt::Horizontal, parent);

    // Root widgets
    ui.m_rootEdit = new MemoryLineEdit("projectRoot", parent);
    ui.m_graphWindow = new GraphWidget();
    ui.m_seqGraphWindow = nullptr;
    ui.browseBtn = new StyledButton(tr_fn("Browse..."), parent);

    // Top row (path + Browse)
    ui.top_widget = new QWidget(parent);
    auto* top = new QHBoxLayout(ui.top_widget);
    top->addWidget(ui.m_rootEdit, 1);
    top->addWidget(ui.browseBtn);

    // File list
    ui.m_files = new QListWidget(parent);

    // Settings panel
    buildSettingsPanel(ui, tr_fn, tts, parent);

    // Log & Chat
    ui.m_log = new StyleTextEdit(parent);
    ui.m_log->setReadOnly(true);
    ui.m_log->setVisible(false);

    ui.m_chat = new ChatDialog(model, parent);

    // Sequence graph widget
    ui.m_seqGraphWindow = new SequenceGraphWidget(parent);
    ui.m_seqGraphWindow->setSequenceManager(seqMgr);
    ui.m_seqGraphWindow->setTemplateManager(tmplMgr);
    ui.m_seqGraphWindow->restoreLayout();
    ui.m_seqGraphWindow->rebuildGraph();
    ui.m_seqGraphWindow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui.m_seqGraphWindow->setVisible(true);

    // Result display widget
    ui.m_resultDisplayWidget = new ResultDisplayWidget(parent);

    // Context status widget
    ui.m_contextStatusWidget = new QWidget(parent);
    ui.m_contextCommandsList = new QTreeWidget(ui.m_contextStatusWidget);
    ui.m_contextCommandsList->setObjectName("contextCommandsList");
    ui.m_contextGraphWidget = new ContextGraphWidget(ui.m_contextStatusWidget);
    ui.m_contextTabs = new QTabWidget(ui.m_contextStatusWidget);
    ui.m_contextTabs->setDocumentMode(true);

    QVBoxLayout* contextLayout = new QVBoxLayout(ui.m_contextStatusWidget);
    contextLayout->setContentsMargins(5, 5, 5, 5);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel(tr_fn("Context Commands:"), ui.m_contextStatusWidget);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    contextLayout->addLayout(headerLayout);
    ui.m_contextTabs->addTab(ui.m_contextCommandsList, tr_fn("Commands"));
    ui.m_contextTabs->addTab(ui.m_contextGraphWidget, tr_fn("Context Graph"));
    contextLayout->addWidget(ui.m_contextTabs);

    ui.m_contextCommandsList->setHeaderLabels(QStringList() << tr_fn("Time") << tr_fn("Description"));
    ui.m_contextCommandsList->setColumnCount(2);
    ui.m_contextCommandsList->setHeaderHidden(false);
    ui.m_contextCommandsList->setRootIsDecorated(true);
    ui.m_contextCommandsList->setColumnWidth(0, 80);

    ui.m_contextStatusWidget->setMinimumWidth(260);
    ui.m_contextStatusWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    // Assemble final layout
    assembleLayout(ui, parent);
}

void UiBuilder::buildSettingsPanel(UIWidgets& ui,
                                   std::function<QString(const char*)> tr_fn,
                                   TtsController* tts,
                                   QWidget* parent)
{
    ui.m_settingsPanel = new QWidget(parent);
    ui.m_settingsPanel->setWindowTitle(tr_fn("Settings"));
    ui.m_settingsPanel->setAttribute(Qt::WA_DeleteOnClose, false);
    ui.m_settingsPanel->setAttribute(Qt::WA_StyledBackground, true);
    ui.m_settingsPanel->setObjectName("settingsPanel");
    ui.m_settingsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* settingsRoot = new QHBoxLayout(ui.m_settingsPanel);
    settingsRoot->setContentsMargins(12, 12, 12, 12);
    settingsRoot->setSpacing(12);

    // Sidebar
    ui.m_settingsSidebar = new QListWidget(parent);
    ui.m_settingsSidebar->setMaximumWidth(180);
    ui.m_settingsSidebar->setMinimumWidth(140);
    ui.m_settingsSidebar->setStyleSheet(
        "QListWidget { background: #fff; border: 1px solid #8ed9cf; border-radius: 8px; padding: 6px; }"
        "QListWidget::item { padding: 8px 12px; border-radius: 6px; margin: 2px 0; font: 9pt \"Segoe UI Variable\"; color: #334155; }"
        "QListWidget::item:selected { background: #dff8f3; color: #0f766e; font-weight: 600; }"
        "QListWidget::item:hover { background: #e6f5f1; }"
    );
    ui.m_settingsSidebar->addItem(QString::fromUtf8("📁  ") + tr_fn("Project"));
    ui.m_settingsSidebar->addItem(QString::fromUtf8("🤖  ") + tr_fn("Model"));
    ui.m_settingsSidebar->addItem(QString::fromUtf8("📋  ") + tr_fn("Sequences"));
    ui.m_settingsSidebar->addItem(QString::fromUtf8("📊  ") + tr_fn("Graphs"));
    ui.m_settingsSidebar->addItem(QString::fromUtf8("🔊  ") + tr_fn("Audio"));
    ui.m_settingsSidebar->addItem(QString::fromUtf8("⚙️  ") + tr_fn("System"));
    settingsRoot->addWidget(ui.m_settingsSidebar, 0);

    // Pages stack
    ui.m_settingsPages = new QStackedWidget(parent);
    ui.m_settingsPages->setStyleSheet("QStackedWidget { background: transparent; }");
    settingsRoot->addWidget(ui.m_settingsPages, 1);
    QObject::connect(ui.m_settingsSidebar, &QListWidget::currentRowChanged,
                     ui.m_settingsPages, &QStackedWidget::setCurrentIndex);

    // Build all pages
    auto* p0 = makePage(); ui.m_settingsPages->addWidget(p0); buildProjectPage(ui, tr_fn, p0, parent);
    auto* p1 = makePage(); ui.m_settingsPages->addWidget(p1); buildModelPage(ui, tr_fn, p1, parent);
    auto* p2 = makePage(); ui.m_settingsPages->addWidget(p2); buildSequencesPage(ui, tr_fn, p2, parent);
    auto* p3 = makePage(); ui.m_settingsPages->addWidget(p3); buildGraphsPage(ui, tr_fn, p3, parent);
    auto* p4 = makePage(); ui.m_settingsPages->addWidget(p4); buildAudioPage(tr_fn, tts, p4);
    auto* p5 = makePage(); ui.m_settingsPages->addWidget(p5); buildSystemPage(ui, tr_fn, p5, parent);

    // Set Expanding policy for all buttons on settings pages
    for (int i = 0; i < ui.m_settingsPages->count(); ++i) {
        auto* page = qobject_cast<QWidget*>(ui.m_settingsPages->widget(i));
        if (page) {
            for (auto* btn : page->findChildren<StyledButton*>())
                btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
    }

    // Hidden buttons
    ui.applyBtn = new StyledButton(tr_fn("Apply"), parent);
    ui.undoBtn = new StyledButton(tr_fn("Undo"), parent);
    ui.applyBtn->hide();
    ui.undoBtn->hide();
    ui.runSeqBtn = new StyledButton(tr_fn("Run"), parent);
    ui.runSeqBtn->hide();
}

QWidget* UiBuilder::makePage()
{
    auto* w = new QWidget();
    w->setStyleSheet("QWidget { background: #f8fffd; }");
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(12, 12, 12, 12);
    l->setSpacing(8);
    l->addStretch();
    return w;
}

QVBoxLayout* UiBuilder::pageLay(QWidget* w)
{
    return static_cast<QVBoxLayout*>(w->layout());
}

void UiBuilder::addSection(const QString& title, QVBoxLayout* layout)
{
    auto* lb = new QLabel(title);
    lb->setStyleSheet("QLabel { color: #0f766e; font: 600 11pt \"Segoe UI Variable\"; padding: 4px 0; }");
    layout->addWidget(lb);
}

void UiBuilder::addDiv(QVBoxLayout* layout)
{
    auto* ln = new QFrame();
    ln->setFrameShape(QFrame::HLine);
    ln->setStyleSheet("QFrame { background: #bdebe4; min-height: 1px; max-height: 1px; border: none; }");
    layout->addWidget(ln);
}

void UiBuilder::buildProjectPage(UIWidgets& ui,
                                 std::function<QString(const char*)> tr_fn,
                                 QWidget* page, QWidget* parent)
{
    auto* projLay = pageLay(page);

    addSection(tr_fn("Project path"), projLay);
    projLay->addWidget(ui.top_widget);
    addDiv(projLay);
    addSection(tr_fn("Actions"), projLay);

    ui.rescanBtn = new StyledButton(tr_fn("Rescan project"), parent);
    ui.descBtn = new StyledButton(tr_fn("Generate descriptions"), parent);
    ui.rescanBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui.descBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    projLay->addWidget(ui.rescanBtn);
    projLay->addWidget(ui.descBtn);
    projLay->addStretch();
}

void UiBuilder::buildModelPage(UIWidgets& ui,
                               std::function<QString(const char*)> tr_fn,
                               QWidget* page, QWidget* parent)
{
    auto* llmLay = pageLay(page);

    addSection(tr_fn("Model settings"), llmLay);

    ui.settingsBtn = new StyledButton(tr_fn("Open LLM settings"), parent);
    ui.promptBtn = new StyledButton(tr_fn("Edit base prompt"), parent);
    llmLay->addWidget(ui.settingsBtn);
    llmLay->addWidget(ui.promptBtn);
    addDiv(llmLay);

    ui.m_llmSettingsWindow = new OllamaSettingsDialog(parent);
    ui.m_llmSettingsWindow->setVisible(false);
    llmLay->addWidget(ui.m_llmSettingsWindow);
    llmLay->addStretch();
}

void UiBuilder::buildSequencesPage(UIWidgets& ui,
                                   std::function<QString(const char*)> tr_fn,
                                   QWidget* page, QWidget* parent)
{
    auto* seqLay = pageLay(page);

    addSection(tr_fn("Sequences"), seqLay);

    ui.editSeqBtn = new StyledButton(tr_fn("Edit"), parent);
    ui.newSeqBtn = new StyledButton(tr_fn("Create"), parent);
    ui.runTemplateBtn = new StyledButton(tr_fn("Run template"), parent);
    ui.editTemplatesBtn = new StyledButton(tr_fn("Edit templates"), parent);
    ui.exportSequenceAsTemplateBtn = new StyledButton(tr_fn("Export as template"), parent);

    seqLay->addWidget(ui.editSeqBtn);
    seqLay->addWidget(ui.newSeqBtn);
    seqLay->addWidget(ui.runTemplateBtn);
    seqLay->addWidget(ui.editTemplatesBtn);
    seqLay->addWidget(ui.exportSequenceAsTemplateBtn);
    seqLay->addStretch();
}

void UiBuilder::buildGraphsPage(UIWidgets& ui,
                                std::function<QString(const char*)> tr_fn,
                                QWidget* page, QWidget* parent)
{
    auto* graphLay = pageLay(page);

    addSection(tr_fn("Visualization"), graphLay);

    ui.graphBtn = new StyledButton(tr_fn("Context graph"), parent);
    ui.seqGraphBtn = new StyledButton(tr_fn("Sequence graph"), parent);
    ui.lightRagBtn = new StyledButton(tr_fn("LightRAG"), parent);
    ui.documentsBtn = new StyledButton(tr_fn("Documents: search and questions"), parent);
    ui.documentsBtn->setToolTip(tr_fn("Index the project's documents and ask questions about them. "
                                      "The agent can then search them with SEARCH_DOCS."));

    graphLay->addWidget(ui.graphBtn);
    graphLay->addWidget(ui.seqGraphBtn);
    graphLay->addWidget(ui.documentsBtn);
    graphLay->addWidget(ui.lightRagBtn);
    graphLay->addStretch();
}

void UiBuilder::buildAudioPage(std::function<QString(const char*)> tr_fn,
                               TtsController* tts, QWidget* page)
{
    auto* audioLay = pageLay(page);

    addSection(tr_fn("Voice input"), audioLay);
    auto* form = new QFormLayout;

    auto* engine = new QComboBox(page);
    for (SpeechRecognizer::Engine e : {SpeechRecognizer::Engine::FasterWhisper, SpeechRecognizer::Engine::Vosk,
                                       SpeechRecognizer::Engine::WhisperCpp, SpeechRecognizer::Engine::OmniAsr})
        engine->addItem(SpeechRecognizer::engineTitle(e), SpeechRecognizer::engineKey(e));
    engine->setCurrentIndex(qMax(0, engine->findData(SpeechRecognizer::engineKey(SpeechRecognizer::configuredEngine()))));
    QObject::connect(engine, QOverload<int>::of(&QComboBox::currentIndexChanged), [engine](int) {
        QSettings().setValue("speech/engine", engine->currentData().toString());
    });
    form->addRow(tr_fn("Engine"), engine);

    // A path setting with a "..." button; `folder` picks a directory instead of a file.
    const auto pathRow = [page, form](const QString& label, const QString& key, bool folder, const QString& hint) {
        auto* row = new QHBoxLayout;
        auto* edit = new QLineEdit(QSettings().value(key).toString(), page);
        edit->setPlaceholderText(hint);
        auto* browse = new QPushButton(QStringLiteral("..."), page);
        browse->setFixedWidth(32);
        row->addWidget(edit, 1);
        row->addWidget(browse);
        QObject::connect(edit, &QLineEdit::editingFinished, [edit, key]() {
            QSettings().setValue(key, edit->text().trimmed());
        });
        QObject::connect(browse, &QPushButton::clicked, [page, edit, key, folder, label]() {
            const QString picked = folder ? QFileDialog::getExistingDirectory(page, label, edit->text())
                                          : QFileDialog::getOpenFileName(page, label, edit->text());
            if (!picked.isEmpty()) {
                edit->setText(QDir::toNativeSeparators(picked));
                QSettings().setValue(key, edit->text());
            }
        });
        form->addRow(label, row);
    };
    pathRow(tr_fn("Vosk library"), "speech/voskDll", false, tr_fn("libvosk.dll"));
    pathRow(tr_fn("Vosk model"), "speech/voskModel", true, tr_fn("folder, e.g. vosk-model-small-ru-0.22"));
    pathRow(tr_fn("whisper.cpp program"), "speech/whisperExe", false, tr_fn("whisper-cli.exe"));
    pathRow(tr_fn("whisper.cpp model"), "speech/whisperModel", false, tr_fn("ggml-small.bin"));
    pathRow(tr_fn("Python"), "speech/python", false, tr_fn("python from PATH"));

    auto* language = new QLineEdit(QSettings().value("speech/language").toString(), page);
    language->setPlaceholderText(tr_fn("detect automatically, or a code such as ru, en"));
    QObject::connect(language, &QLineEdit::editingFinished, [language]() {
        QSettings().setValue("speech/language", language->text().trimmed());
    });
    form->addRow(tr_fn("Language (whisper.cpp)"), language);
    audioLay->addLayout(form);

    addDiv(audioLay);
    addSection(tr_fn("Text to speech"), audioLay);
    audioLay->addWidget(tts->toggleButton());
    audioLay->addStretch();
}

void UiBuilder::buildSystemPage(UIWidgets& ui,
                                std::function<QString(const char*)> tr_fn,
                                QWidget* page, QWidget* parent)
{
    auto* sysLay = pageLay(page);

    addSection(tr_fn("Security"), sysLay);

    // New key on purpose: the old "confirmMode" default was "run everything", and a stored
    // value from those days must not carry over.
    ui.confirmModeCombo = new MemoryComboBox("commandConfirmMode", parent);
    ui.confirmModeCombo->addItem(tr_fn("Run read-only commands, ask for the rest"), "readonly");
    ui.confirmModeCombo->addItem(tr_fn("Ask before every command"), "ask");
    ui.confirmModeCombo->addItem(tr_fn("Run everything without asking (unsafe)"), "auto");
    ui.confirmModeCombo->setCurrentIndex(0);
    ui.confirmModeCombo->setToolTip(tr_fn("Read-only commands open and search files. Commands that "
                                          "change files, run programs or delete anything are confirmed "
                                          "in a dialog that shows exactly what they will do."));
    sysLay->addWidget(ui.confirmModeCombo);

    addDiv(sysLay);
    addSection(tr_fn("Agent mode"), sysLay);
    auto* agentMode = new QCheckBox(tr_fn("Plan each step with a separate model call (slower, no streaming)"), parent);
    agentMode->setChecked(QSettings().value("agent/enabled", false).toBool());
    agentMode->setToolTip(tr_fn("Each step first asks the model for a plan and then for the commands. "
                                "It uses the model selected in the chat."));
    QObject::connect(agentMode, &QCheckBox::toggled, [](bool on) { QSettings().setValue("agent/enabled", on); });
    sysLay->addWidget(agentMode);

    addDiv(sysLay);
    addSection(tr_fn("Interface"), sysLay);

    ui.chatToggleBtn = new StyledButton(tr_fn("Show chat"), parent);
    ui.logToggleBtn = new StyledButton(tr_fn("Show log"), parent);
    ui.chatToggleBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui.logToggleBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sysLay->addWidget(ui.chatToggleBtn);
    sysLay->addWidget(ui.logToggleBtn);
    sysLay->addStretch();
}

void UiBuilder::assembleLayout(UIWidgets& ui, QWidget* parent)
{
    ui.m_settingsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Center splitter (vertical)
    ui.m_centerSplitter = new MemorySplitter("MainWindowsCenterSplitter", Qt::Vertical, parent);
    ui.m_centerSplitter->addWidget(ui.ide_widget);
    ui.m_centerSplitter->addWidget(ui.m_log);
    ui.m_centerSplitter->addWidget(ui.m_resultDisplayWidget);
    ui.m_centerSplitter->addWidget(ui.m_chat);

    ui.mainSplitter = new MemorySplitter("MainWindowsMainSplitter", Qt::Horizontal, parent);
    ui.mainSplitter->addWidget(ui.m_contextStatusWidget);
    ui.mainSplitter->addWidget(ui.m_centerSplitter);
    ui.mainSplitter->addWidget(ui.m_seqGraphWindow);

    ui.m_centerSplitter->setStretchFactor(0, 5);
    ui.m_centerSplitter->setStretchFactor(1, 1);
    ui.m_centerSplitter->setStretchFactor(2, 3);
    ui.m_centerSplitter->setStretchFactor(3, 2);
    ui.mainSplitter->setStretchFactor(0, 1);
    ui.mainSplitter->setStretchFactor(1, 3);
    ui.mainSplitter->setStretchFactor(2, 2);

    ui.m_files->hide();
}
