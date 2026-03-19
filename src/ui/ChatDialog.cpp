#include "ChatDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QGridLayout>
#include <QDirIterator>
#include <QStandardPaths>
#include <QFileInfo>
#include <QFile>
#include <QFileDialog>
#include <QToolButton>
#include <QBuffer>
#include <QImageReader>
#include <QRegularExpression>
#include <QJsonArray>
#include "../core/SolutionTemplateManager.h"
#include "../core/SolutionTemplate.h"
#include "../utils/AppConfig.h"
#include "MainWindow.h"

static QStringList asrSendTriggers()
{
    return QStringList{
        "отправить", "отправь", "готово", "выполни", "выполнить", "поехали", "давай",
        "send", "submit", "done", "go", "execute"
    };
}

static QString normalizeAsrText(QString text)
{
    text = text.trimmed();
    text.replace(QRegularExpression("[\\s\\t\\n\\r]+"), " ");
    return text;
}

ChatDialog::ChatDialog(OllamaAdapter* model, QWidget* parent):QWidget(parent),m_model(model){
    setWindowTitle(tr("Chat / Instruction"));
    resize(680,480);
    auto v = new QVBoxLayout(this);
    m_chat = new StyleTextEdit(this);
    m_chat->setReadOnly(true);
    m_chat->setVisible(false);
    m_cb_model = new MemoryComboBox("modelSelector", this);
    v->addWidget(m_chat,1);
    auto mainLayout = new QHBoxLayout();
    m_input = new InputTextEdit(this);
    m_send = new StyledButton("Отправить", this);
    m_stop = new StyledButton("Остановить", this);
    m_micBtn = new StyledButton("🎤", this);
    m_micBtn->setFixedSize(36, 36);
    m_micBtn->setToolTip("Голосовой ввод (нажмите для записи)");
    m_micBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #0f766e;
            color: #ecfeff;
            border: 1px solid #14b8a6;
            border-radius: 18px;
            font-size: 15px;
        }
        QPushButton:hover {
            background-color: #115e59;
        }
        QPushButton:pressed {
            background-color: #134e4a;
        }
    )");

    m_attachBtn = new StyledButton("Image", this);
    m_attachBtn->setFixedSize(44, 36);
    m_attachBtn->setToolTip("Attach image (png/jpg/webp)");
    
    // Left column: input + image attachments bar
    auto leftColumnLayout = new QVBoxLayout();
    leftColumnLayout->setContentsMargins(0, 0, 0, 0);
    leftColumnLayout->setSpacing(6);
    leftColumnLayout->addWidget(m_input, 1);

    m_imagesScroll = new QScrollArea(this);
    m_imagesScroll->setWidgetResizable(true);
    m_imagesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_imagesScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_imagesScroll->setFixedHeight(92);
    m_imagesScroll->setVisible(false);

    m_imagesBar = new QWidget(m_imagesScroll);
    m_imagesLayout = new QHBoxLayout(m_imagesBar);
    m_imagesLayout->setContentsMargins(6, 6, 6, 6);
    m_imagesLayout->setSpacing(8);
    m_imagesLayout->addStretch(1);
    m_imagesScroll->setWidget(m_imagesBar);

    leftColumnLayout->addWidget(m_imagesScroll, 0);
    mainLayout->addLayout(leftColumnLayout, 1);
    
    // Create a vertical layout for ALL controls to the right of input
    auto rightColumnLayout = new QVBoxLayout();
    
    // Create a horizontal layout for the send/stop buttons
    auto buttonsLayout = new QHBoxLayout();
    buttonsLayout->addWidget(m_send);
    buttonsLayout->addWidget(m_stop);
    buttonsLayout->addWidget(m_attachBtn);
    buttonsLayout->addWidget(m_micBtn);
    rightColumnLayout->addLayout(buttonsLayout);
    
    // Add the template selection combobox below the buttons
    m_templateCombo = new MemoryComboBox("defaultTemplateSelector", this);
    m_templateCombo->addItem("Выберите шаблон по умолчанию", "");
    m_templateCombo->addItem("Простой чат", "simple_chat");
    m_templateCombo->setItemData(1, QColor(255, 255, 200), Qt::BackgroundColorRole); // Light yellow background
    m_templateCombo->addItems(QStringList()); // ensure initial

    // Populate the combobox with all available templates
    populateTemplates();
    m_templateCombo->loadState();
    notifyTemplateSelectionChanged();

    rightColumnLayout->addWidget(m_templateCombo);

    connect(m_templateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){
        notifyTemplateSelectionChanged();
    });
    
    // Finally add the model selection combobox at the very bottom
    rightColumnLayout->addWidget(m_cb_model);
    
    // Add the right column with all controls
    mainLayout->addLayout(rightColumnLayout);
    v->addLayout(mainLayout);
    
    connect(m_send, &StyledButton::clicked, this, &ChatDialog::onSend);
    connect(m_stop, &StyledButton::clicked, this, &ChatDialog::onStop);
    connect(m_input, &InputTextEdit::sendMessage, this, &ChatDialog::onSend);
    connect(m_attachBtn, &StyledButton::clicked, this, &ChatDialog::onAttachImages);
    connect(m_input, &InputTextEdit::imagesInserted, this, &ChatDialog::onImagesInserted);

    connect(m_cb_model, &MemoryComboBox::currentTextChanged, this, [this](const QString &text){
        if (!m_model)
            return;

        AppConfig cfg;
        cfg.load();
        QVariantMap params = cfg.params;
        if (params.isEmpty()) {
            params["temperature"] = cfg.temperature;
            params["top_p"] = cfg.topP;
            params["top_k"] = cfg.topK;
            params["repeat_penalty"] = cfg.repeatPenalty;
            params["presence_penalty"] = cfg.presencePenalty;
            params["frequency_penalty"] = cfg.frequencyPenalty;
            params["num_predict"] = cfg.numPredict;
            params["num_ctx"] = cfg.numCtx;
            params["mirostat"] = cfg.mirostat;
            params["mirostat_tau"] = cfg.mirostatTau;
            params["mirostat_eta"] = cfg.mirostatEta;
            params["seed"] = cfg.seed;
            params["reasoning_effort"] = cfg.reasoningEffort;
            params["temperature_decay"] = cfg.temperatureDecay;
            params["repeat_last_n"] = cfg.repeatLastN;

            if (!cfg.stopWords.isEmpty()) {
                QJsonArray stopArray;
                for (const QString& word : cfg.stopWords)
                    stopArray.append(word);
                params["stop"] = stopArray;
            }
        }

        m_model->configure(text, params);
        cfg.params = params;

        cfg.selectedModel = text;
        cfg.provider = m_model->providerName();
        cfg.save();
    });

    pslot_updateModelsList();

    connect(m_input,  &InputTextEdit::signal_textEdited, this, [this](const QString &str){

        emit signal_updatelastUserMessage(str);
    });

    m_whisperRunner = new FasterWhisperRunner(this);
    
    connect(m_micBtn, &StyledButton::clicked, this, &ChatDialog::onMicClicked);
    
    connect(m_whisperRunner, &FasterWhisperRunner::recognitionFinished, this, &ChatDialog::onAsrFinished);
    connect(m_whisperRunner, &FasterWhisperRunner::errorOccurred, this, &ChatDialog::onAsrError);
    connect(m_whisperRunner, &FasterWhisperRunner::silenceDetected, this, &ChatDialog::onSilenceDetected);
      connect(m_whisperRunner, &FasterWhisperRunner::statusMessage, this, [this](const QString& msg){
        if (m_chat) {
            m_chat->setVisible(true); // Ensure chat is visible
            m_chat->append("<i>" + msg + "</i>");
        }
    });
}

bool ChatDialog::modelSupportsVision() const
{
    if (!m_model)
        return false;

    const QString model = m_model->selectedModel().toLower().trimmed();
    if (model.isEmpty())
        return false;

    static const QStringList needles = {
        "llava",
        "vision",
        "minicpm-v",
        "qwen2-vl",
        "qwen2.5-vl",
        "qwen3-vl",
        "pixtral",
        "moondream",
        "bakllava",
        "-vl",
        ":vl",
        "vl-"
    };

    for (const QString& n : needles) {
        if (model.contains(n))
            return true;
    }
    return false;
}

void ChatDialog::onAttachImages()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Select images"),
        QString(),
        tr("Images (*.png *.jpg *.jpeg *.webp *.bmp);;All files (*.*)"));

    if (files.isEmpty())
        return;

    addImageAttachmentsFromFiles(files);
}

void ChatDialog::onImagesInserted(const QStringList &filePaths,
                                 const QStringList &imagesBase64Png,
                                 const QStringList &imageNames)
{
    if (!filePaths.isEmpty()) {
        addImageAttachmentsFromFiles(filePaths);
    }
    if (!imagesBase64Png.isEmpty()) {
        addImageAttachmentsFromBase64(imageNames, imagesBase64Png);
    }
}

void ChatDialog::addImageAttachmentsFromFiles(const QStringList &files)
{
    for (const QString& path : files) {
        QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile())
            continue;

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            continue;

        const QByteArray bytes = f.readAll();
        f.close();
        if (bytes.isEmpty())
            continue;

        QImageReader reader(path);
        reader.setAutoTransform(true);
        QImage img = reader.read();
        if (img.isNull()) {
            img = QImage::fromData(bytes);
        }
        if (img.isNull())
            continue;

        ImageAttachment att;
        att.filePath = QDir::toNativeSeparators(path);
        att.fileName = fi.fileName();
        att.base64 = QString::fromLatin1(bytes.toBase64());
        att.thumb = QPixmap::fromImage(img.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_imageAttachments.push_back(att);
    }

    rebuildImagesBar();
}

void ChatDialog::addImageAttachmentsFromBase64(const QStringList &imageNames, const QStringList &imagesBase64)
{
    const int n = imagesBase64.size();
    for (int i = 0; i < n; ++i) {
        const QString b64 = imagesBase64[i].trimmed();
        if (b64.isEmpty())
            continue;

        const QByteArray bytes = QByteArray::fromBase64(b64.toLatin1());
        if (bytes.isEmpty())
            continue;

        QImage img = QImage::fromData(bytes);
        if (img.isNull())
            continue;

        const QString name = (i < imageNames.size() && !imageNames[i].trimmed().isEmpty())
                                 ? imageNames[i].trimmed()
                                 : QString("clipboard_image_%1.png").arg(i + 1);

        ImageAttachment att;
        att.fileName = name;
        att.filePath = QString("clipboard:%1").arg(name);
        att.base64 = b64;
        att.thumb = QPixmap::fromImage(img.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_imageAttachments.push_back(att);
    }

    rebuildImagesBar();
}

void ChatDialog::rebuildImagesBar()
{
    if (!m_imagesLayout || !m_imagesBar || !m_imagesScroll)
        return;

    while (m_imagesLayout->count() > 0) {
        QLayoutItem* item = m_imagesLayout->takeAt(0);
        if (!item)
            break;
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    for (int i = 0; i < m_imageAttachments.size(); ++i) {
        const ImageAttachment& att = m_imageAttachments[i];

        QWidget* itemWidget = new QWidget(m_imagesBar);
        itemWidget->setFixedSize(86, 86);

        auto grid = new QGridLayout(itemWidget);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(0);

        QLabel* pic = new QLabel(itemWidget);
        pic->setPixmap(att.thumb);
        pic->setAlignment(Qt::AlignCenter);
        pic->setToolTip(att.filePath);
        pic->setFixedSize(86, 86);
        pic->setStyleSheet("border: 1px solid #4a4a4a; border-radius: 6px; background: #222;");
        grid->addWidget(pic, 0, 0, Qt::AlignCenter);

        QToolButton* removeBtn = new QToolButton(itemWidget);
        removeBtn->setText("x");
        removeBtn->setToolTip(tr("Remove"));
        removeBtn->setFixedSize(18, 18);
        removeBtn->setStyleSheet(
            "QToolButton { background: rgba(0,0,0,180); color: white; border: 1px solid #666; border-radius: 9px; }"
            "QToolButton:hover { border-color: #aaa; }");
        grid->addWidget(removeBtn, 0, 0, Qt::AlignTop | Qt::AlignRight);

        connect(removeBtn, &QToolButton::clicked, this, [this, i]() {
            if (i < 0 || i >= m_imageAttachments.size())
                return;
            m_imageAttachments.removeAt(i);
            rebuildImagesBar();
        });

        m_imagesLayout->addWidget(itemWidget);
    }

    m_imagesLayout->addStretch(1);
    m_imagesScroll->setVisible(!m_imageAttachments.isEmpty());
}

void ChatDialog::clearImageAttachments()
{
    m_imageAttachments.clear();
    rebuildImagesBar();
}

QStringList ChatDialog::ensureImageSummaries(QString* combinedOut)
{
    QStringList summaries;
    if (m_imageAttachments.isEmpty())
        return summaries;

    bool allHave = true;
    for (const auto& a : m_imageAttachments) {
        if (a.summary.trimmed().isEmpty()) {
            allHave = false;
            break;
        }
    }
    if (allHave) {
        for (const auto& a : m_imageAttachments)
            summaries << a.summary;
        return summaries;
    }

    if (!modelSupportsVision() || !m_model)
        return summaries;

    QStringList imagesBase64;
    for (const auto& a : m_imageAttachments)
        imagesBase64 << a.base64;

    const int n = imagesBase64.size();
    QString prompt =
        "Describe each image. Give 1-2 sentences per image.\n"
        "Strict format:\n";
    for (int i = 1; i <= n; ++i)
        prompt += QString("%1) ...\n").arg(i);

    QVariantMap options;
    options.insert("temperature", 0.2);
    options.insert("num_predict", 256);
    options.insert("timeoutMs", 9000);

    const QString resp = m_model->generateWithSystemAndImages(prompt, QString(), imagesBase64, options).trimmed();
    if (combinedOut)
        *combinedOut = resp;

    if (resp.isEmpty())
        return summaries;

    QMap<int, QString> parsed;
    const auto lines = resp.split('\n', QString::SkipEmptyParts);
    QRegularExpression re("^\\s*(\\d+)\\s*[\\).:-]\\s*(.+)\\s*$");
    for (const QString& line : lines) {
        QRegularExpressionMatch m = re.match(line);
        if (!m.hasMatch())
            continue;
        const int idx = m.captured(1).toInt();
        const QString text = m.captured(2).trimmed();
        if (idx > 0 && !text.isEmpty())
            parsed[idx] = text;
    }

    summaries.reserve(n);
    for (int i = 1; i <= n; ++i) {
        const QString s = parsed.value(i).trimmed();
        summaries << (s.isEmpty() ? QString() : s);
    }

    bool anyNonEmpty = false;
    for (const QString& s : summaries) {
        if (!s.isEmpty()) {
            anyNonEmpty = true;
            break;
        }
    }
    if (!anyNonEmpty) {
        summaries.clear();
        for (int i = 0; i < n; ++i)
            summaries << resp;
    }

    for (int i = 0; i < m_imageAttachments.size() && i < summaries.size(); ++i)
        m_imageAttachments[i].summary = summaries[i];

    return summaries;
}

QString ChatDialog::buildAugmentedTextWithImages(const QString& text, const QVector<ImageAttachment>& attachments) const
{
    if (attachments.isEmpty())
        return text;

    QString out = text;
    out += "\n\n[Images]\n";
    for (const auto& a : attachments) {
        if (!a.summary.trimmed().isEmpty()) {
            out += QString("- %1: %2\n").arg(a.fileName, a.summary.trimmed());
        } else {
            out += QString("- %1\n").arg(a.fileName);
        }
    }
    return out.trimmed();
}

void ChatDialog::onMicClicked() {
    if (m_isRecording) {
        m_whisperRunner->stopRecording();
        
        // Switch to "not recording" state - blue microphone
        m_micBtn->setText("🎤");
        m_micBtn->setToolTip("Голосовой ввод (нажмите для записи)");
        m_micBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #0f766e;
                color: #ecfeff;
                border: 1px solid #14b8a6;
                border-radius: 18px;
                font-size: 15px;
            }
            QPushButton:hover {
                background-color: #115e59;
            }
        )");
        m_isRecording = false;
        
        // Hide chat log if it was only showing status
        if (m_chat && m_chat->toPlainText().contains("Faster Whisper")) {
            m_chat->setVisible(false);
            m_chat->clear();
        }
    } else {
        // Use Faster Whisper by default for streaming experience
        m_whisperRunner->startRecording();
        
        // Switch to "recording" state - red crossed microphone
        m_micBtn->setText("🎙️");
        m_micBtn->setToolTip("Идёт запись... (нажмите для остановки)");
        m_micBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #b91c1c;
                color: #fef2f2;
                border: 1px solid #ef4444;
                border-radius: 18px;
                font-size: 15px;
            }
            QPushButton:hover {
                background-color: #991b1b;
            }
        )");
        m_isRecording = true;
    }
}

void ChatDialog::onAsrFinished(const QString& text) {
    const QString normalizedText = normalizeAsrText(text);
    if (normalizedText.isEmpty()) {
        return;
    }
    
    // Add text to input
    setInputText(normalizedText);
    
    // Check for trigger phrases - send immediately
    if (checkTriggerPhrase(normalizedText)) {
        // Remove trigger phrase from the end
        QString inputText = m_input->toPlainText().trimmed();
        const QString lowerInput = inputText.toLower();
        const QStringList triggers = asrSendTriggers();
        for (const QString& trigger : triggers) {
            if (lowerInput.endsWith(trigger)) {
                inputText = inputText.left(inputText.length() - trigger.length()).trimmed();
                m_input->setPlainText(inputText);
                break;
            }
        }
        onSend();
        // Don't stop recording - keep mic active for continuous conversation
        return;
    }
}

void ChatDialog::onSilenceDetected() {
    // After 3 seconds of silence, send the message
    QString currentText = m_input->toPlainText().trimmed();
    if (!currentText.isEmpty()) {
        onSend();
    }
}

bool ChatDialog::checkTriggerPhrase(const QString& text) {
    QString lower = normalizeAsrText(text).toLower();
    lower.remove(QRegularExpression(R"([.!?,;:]+$)"));
    const QStringList triggers = asrSendTriggers();
    for (const QString& trigger : triggers) {
        if (lower.endsWith(trigger) || lower == trigger) {
            return true;
        }
    }
    return false;
}



void ChatDialog::onAsrError(const QString& msg) {
    m_micBtn->setText("🎤");
    m_micBtn->setEnabled(true);
    m_micBtn->setStyleSheet(""); // Reset style
    m_isRecording = false;
    // Show error in chat or log
    m_chat->append("ASR Error: " + msg);
}

void ChatDialog::resumeRecordingAfterResponse() {
    // If recording was active, ensure it continues
    // This is called after TTS finishes speaking the response
    if (m_isRecording) {
        // Recording is still active, nothing to do
        return;
    }
    // If user had mic active before, we could auto-resume here
    // For now, just ensure the button is ready
    m_micBtn->setEnabled(true);
}

void ChatDialog::onStop(){
    if (m_model) m_model->stopStream();
    // Also signal the MainWindow to stop template execution if running
    if (m_mainWnd) {
        m_mainWnd->stopTemplateExecution();
    }
}

void ChatDialog::pslot_updateModelsList(){
    if (!m_model || !m_cb_model)
        return;

    AppConfig cfg;
    cfg.load();
    QVariantMap params = cfg.params;
    if (params.isEmpty()) {
        params["temperature"] = cfg.temperature;
        params["top_p"] = cfg.topP;
        params["top_k"] = cfg.topK;
        params["repeat_penalty"] = cfg.repeatPenalty;
        params["presence_penalty"] = cfg.presencePenalty;
        params["frequency_penalty"] = cfg.frequencyPenalty;
        params["num_predict"] = cfg.numPredict;
        params["num_ctx"] = cfg.numCtx;
        params["mirostat"] = cfg.mirostat;
        params["mirostat_tau"] = cfg.mirostatTau;
        params["mirostat_eta"] = cfg.mirostatEta;
        params["seed"] = cfg.seed;
        params["reasoning_effort"] = cfg.reasoningEffort;
        params["temperature_decay"] = cfg.temperatureDecay;
        params["repeat_last_n"] = cfg.repeatLastN;
        if (!cfg.stopWords.isEmpty()) {
            QJsonArray stopArray;
            for (const QString& word : cfg.stopWords)
                stopArray.append(word);
            params["stop"] = stopArray;
        }
    }

    QString preferredModel;
    if (cfg.provider.trimmed().isEmpty() || cfg.provider == m_model->providerName())
        preferredModel = cfg.selectedModel.trimmed();
    if (preferredModel.isEmpty())
        preferredModel = m_model->selectedModel().trimmed();

    QStringList models = m_model->listModels();
    m_cb_model->setItems(models);

    int selectedIdx = -1;
    if (!preferredModel.isEmpty())
        selectedIdx = m_cb_model->findText(preferredModel, Qt::MatchExactly);
    if (selectedIdx < 0 && m_cb_model->count() > 0)
        selectedIdx = 0;

    if (selectedIdx >= 0) {
        m_cb_model->setCurrentIndex(selectedIdx);
        const QString selected = m_cb_model->currentText().trimmed();
        if (!selected.isEmpty()) {
            m_model->configure(selected, params);
            cfg.selectedModel = selected;
            cfg.provider = m_model->providerName();
            cfg.params = params;
            cfg.save();
        }
    } else {
        m_model->configure(QString(), params);
        if (cfg.provider == m_model->providerName()) {
            cfg.selectedModel.clear();
            cfg.params = params;
            cfg.save();
        }
    }

}

void ChatDialog::refreshModelList()
{
    pslot_updateModelsList();
}

void ChatDialog::appendAssistantChunk(const QString& s){
    static bool first = true;
    if (first) { m_chat->append("Assistant: \n"); first=false; }
    m_chat->moveCursor(QTextCursor::End);
    m_chat->insertPlainText(s);
}

void ChatDialog::setChatLogVisible(bool visible)
{
    if (m_chat) {
        m_chat->setVisible(visible);
        if (m_mainWnd) m_mainWnd->updateChatToggleButton(visible);
    }
    notifyTemplateSelectionChanged();
}

bool ChatDialog::isChatLogVisible() const
{
    return m_chat ? m_chat->isVisible() : false;
}

void ChatDialog::finishAssistantMessage(){
    m_chat->append("\n<< stream finished >>");
    appendAssistantChunk("");
}

void ChatDialog::populateTemplates()
{
    // Refresh templates from manager to ensure we have the latest
    if (m_templateManager) {
        m_templateManager->refresh();
        
        // Use templateDirs() to get all template directory paths
        const QStringList& templateDirs = m_templateManager->templateDirs();
        for (const QString& templateDir : templateDirs) {
            // Get the template name from the directory name
            QString templateName = QFileInfo(templateDir).fileName();
            
            // Check if this template is already in the combobox to avoid duplicates
            int existingIndex = m_templateCombo->findData(templateDir);
            if (existingIndex == -1) {  // Not found
                m_templateCombo->addItem(templateName, templateDir);
            }
        }
    } else {
        // Fallback: First, let's find where the templates are stored
        // The templates are likely in the SolutionTemplates directory
        QString templateRoot = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                             .append("/DevelopersAssistant/SolutionTemplates");
        
        // Look for templates in a few common locations
        QStringList possiblePaths;
        possiblePaths << templateRoot 
                      << "SolutionTemplates"  // relative path in project
                      << QDir::current().absolutePath() + "/SolutionTemplates";
        
        for (const QString& path : possiblePaths) {
            QDir templateDir(path);
            if (templateDir.exists()) {
                // Find all subdirectories (these should be the templates)
                QFileInfoList dirList = templateDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QFileInfo& dirInfo : dirList) {
                    QString templatePath = dirInfo.absoluteFilePath();
                    
                    // Check if this is a real template directory by looking for template.templ.json
                    QString templateJsonPath = templatePath + "/template.templ.json";
                    if (QFile::exists(templateJsonPath)) {
                        QString templateName = dirInfo.fileName();
                        // Check if this template is already in the combobox to avoid duplicates
                        int existingIndex = m_templateCombo->findData(templatePath);
                        if (existingIndex == -1) {  // Not found
                            m_templateCombo->addItem(templateName, templatePath);
                        }
                    } else {
                        // If there's no template.templ.json, check if there are sequence files
                        QDir dir(templatePath);
                        QStringList sequenceFiles = dir.entryList(QStringList() << "*.seq.json", QDir::Files);
                        if (!sequenceFiles.isEmpty()) {
                            QString templateName = dirInfo.fileName();
                            // Check if this template is already in the combobox to avoid duplicates
                            int existingIndex = m_templateCombo->findData(templatePath);
                            if (existingIndex == -1) {  // Not found
                                m_templateCombo->addItem(templateName, templatePath);
                            }
                        }
                    }
                }
            }
        }
    }
}

void ChatDialog::onSend(){
    QString t = m_input->toPlainText().trimmed();
    if (t.isEmpty()) return;

    const bool hasImages = !m_imageAttachments.isEmpty();
    QStringList imagesBase64;
    QStringList imagePaths;
    if (hasImages) {
        for (const auto& a : m_imageAttachments) {
            imagePaths << a.filePath;
            imagesBase64 << a.base64;
        }
    }

    QStringList imageSummaries;
    if (hasImages) {
        imageSummaries = ensureImageSummaries(nullptr);
    }
      
    // Get the current selection from the template combobox
    QString selectedTemplateData = m_templateCombo->currentData().toString();
    QString selectedTemplateName = m_templateCombo->currentText();
    Q_UNUSED(selectedTemplateName);

    const QString augmented = hasImages ? buildAugmentedTextWithImages(t, m_imageAttachments) : t;
      
    if (selectedTemplateData == "simple_chat") {
        // Direct chat mode - send directly to model in streaming mode
        // We'll let MainWindow handle displaying the message in the chat
        m_input->clear();

        if (hasImages) {
            emit runSimpleChatWithImages(t, imagePaths, imagesBase64, imageSummaries);
            clearImageAttachments();
        } else {
            // Emit signal for simple chat mode
            emit runSimpleChat(t);
        }
        return;
    }
    else if (selectedTemplateData.isEmpty()) {
        // Default behavior - open template/sequence selection dialog
        // We'll let MainWindow handle displaying the message in the chat
        m_input->clear();
        if (hasImages) {
            emit sendMessageWithImages(augmented, imagePaths, imagesBase64, imageSummaries);
            clearImageAttachments();
        } else {
            emit sendMessage(t);
        }
        return;
    }
    else {
        // A specific template was selected - emit signal to run this template
        // We'll let MainWindow handle displaying the message in the chat
        m_input->clear();
        
        // Emit signal to run the selected template
        if (hasImages) {
            emit runTemplateRequestedWithImages(selectedTemplateData, augmented, imagePaths, imagesBase64, imageSummaries);
            clearImageAttachments();
        } else {
            emit runTemplateRequested(selectedTemplateData, t);
        }
        return;
    }
}

void ChatDialog::appendUserMessage(const QString &s)
{
    m_chat->append(QString("You: %1\n\n").arg(s));
}

void ChatDialog::setTemplateManager(class SolutionTemplateManager* templateManager)
{
    m_templateManager = templateManager;
    
    // Save the current selection before clearing
    QString currentSelection = m_templateCombo->currentData().toString();
    QString currentText = m_templateCombo->currentText();
    
    // Repopulate the templates with the new manager
    m_templateCombo->clear();
    m_templateCombo->addItem("Выберите шаблон по умолчанию", "");
    m_templateCombo->addItem("Простой чат", "simple_chat");
    m_templateCombo->setItemData(1, QColor(255, 255, 200), Qt::BackgroundColorRole); // Light yellow background
    populateTemplates();

    // Restore persisted selection first (cross-session stable behavior).
    m_templateCombo->loadState();

    // Restore the previous selection if it still exists, otherwise keep default
    if (!currentSelection.isEmpty() && m_templateCombo->currentData().toString().isEmpty()) {
        int index = m_templateCombo->findData(currentSelection);
        if (index != -1) {
            m_templateCombo->setCurrentIndex(index);
        } else {
            // Check by text if data doesn't match but text does
            index = m_templateCombo->findText(currentText);
            if (index != -1) {
                m_templateCombo->setCurrentIndex(index);
            }
        }
    }
}

void ChatDialog::notifyTemplateSelectionChanged()
{
    if (!m_templateCombo)
        return;

    QString templatePath = m_templateCombo->currentData().toString();
    if (templatePath == "simple_chat")
        templatePath.clear();

    emit templateSelectionChanged(templatePath);
}

void ChatDialog::sendMessageToChat(const QString &text)
{
    emit sendMessage(text);
}

void ChatDialog::setInputText(const QString &text)
{
    if (m_input) {
        // Insert text at cursor position instead of replacing
        m_input->insertPlainText(text + " ");
        m_input->setFocus();
    }
}

void ChatDialog::setMainWindow(class MainWindow* mainWnd)
{
    m_mainWnd = mainWnd;
}
