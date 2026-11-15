#include "ui/ChatPanel.h"
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QMap>
#include <QRegularExpression>
#include <QColor>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include "audio/SpeechRecognizer.h"
#include "core/model/LlmClient.h"
#include "core/util/AppConfig.h"
#include "core/workflow/SolutionTemplateManager.h"
#include "ui/InputTextEdit.h"
#include "ui/widgets/MemoryComboBox.h"
#include "ui/widgets/StyleTextEdit.h"
#include "ui/widgets/StyledButton.h"

namespace {

const char* const kSimpleChat = "simple_chat";

// Spoken words that send the message, in Russian and English. Matched against the end of
// the recognized text, so they work whatever the interface language is.
const QStringList& sendTriggers()
{
    static const QStringList triggers = {
        QStringLiteral("отправить"), QStringLiteral("отправь"), QStringLiteral("готово"),
        QStringLiteral("выполни"), QStringLiteral("выполнить"), QStringLiteral("поехали"), QStringLiteral("давай"),
        QStringLiteral("send"), QStringLiteral("send it"), QStringLiteral("submit"), QStringLiteral("done"),
        QStringLiteral("go"), QStringLiteral("execute")
    };
    return triggers;
}

QString normalizeSpeech(QString text)
{
    return text.trimmed().replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
}

const char* const kMicIdleStyle = R"(
    QPushButton { background-color: #0f766e; color: #ecfeff; border: 1px solid #14b8a6;
                  border-radius: 18px; font-size: 15px; }
    QPushButton:hover { background-color: #115e59; }
    QPushButton:pressed { background-color: #134e4a; })";
const char* const kMicRecordingStyle = R"(
    QPushButton { background-color: #b91c1c; color: #fef2f2; border: 1px solid #ef4444;
                  border-radius: 18px; font-size: 15px; }
    QPushButton:hover { background-color: #991b1b; })";

} // namespace

ChatPanel::ChatPanel(LlmClient& model, SolutionTemplateManager& templates, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_templates(templates)
{
    auto* layout = new QVBoxLayout(this);
    m_chat = new StyleTextEdit(this);
    m_chat->setReadOnly(true);
    m_chat->setVisible(false);
    layout->addWidget(m_chat, 1);

    // Left: the input and the attached images. Right: the buttons and the two lists.
    auto* row = new QHBoxLayout();
    auto* left = new QVBoxLayout();
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(6);
    m_input = new InputTextEdit(this);
    left->addWidget(m_input, 1);

    m_imagesScroll = new QScrollArea(this);
    m_imagesScroll->setWidgetResizable(true);
    m_imagesScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_imagesScroll->setFixedHeight(92);
    m_imagesScroll->setVisible(false);
    m_imagesBar = new QWidget(m_imagesScroll);
    m_imagesLayout = new QHBoxLayout(m_imagesBar);
    m_imagesLayout->setContentsMargins(6, 6, 6, 6);
    m_imagesLayout->setSpacing(8);
    m_imagesScroll->setWidget(m_imagesBar);
    left->addWidget(m_imagesScroll, 0);
    row->addLayout(left, 1);

    auto* right = new QVBoxLayout();
    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(6);
    m_send = new StyledButton(tr("Send"), this);
    m_stop = new StyledButton(tr("Stop"), this);
    m_attachBtn = new StyledButton(tr("Image"), this);
    m_attachBtn->setFixedHeight(36);
    m_attachBtn->setToolTip(tr("Attach image (png/jpg/webp)"));
    m_micBtn = new StyledButton(QStringLiteral("🎤"), this);
    m_micBtn->setFixedSize(36, 36);
    m_micBtn->setStyleSheet(QLatin1String(kMicIdleStyle));
    m_micBtn->setToolTip(tr("Voice input (click to record)"));
    for (QWidget* w : {static_cast<QWidget*>(m_send), static_cast<QWidget*>(m_stop),
                       static_cast<QWidget*>(m_attachBtn), static_cast<QWidget*>(m_micBtn)})
        buttons->addWidget(w, 0, Qt::AlignVCenter);
    right->addLayout(buttons);

    m_templateCombo = new MemoryComboBox(QStringLiteral("defaultTemplateSelector"), this);
    right->addWidget(m_templateCombo);
    m_modelCombo = new MemoryComboBox(QStringLiteral("modelSelector"), this);
    right->addWidget(m_modelCombo);
    row->addLayout(right);
    layout->addLayout(row);

    refreshTemplates();

    connect(m_send, &StyledButton::clicked, this, &ChatPanel::onSend);
    connect(m_input, &InputTextEdit::sendMessage, this, &ChatPanel::onSend);
    connect(m_stop, &StyledButton::clicked, this, &ChatPanel::stopRequested);
    connect(m_attachBtn, &StyledButton::clicked, this, [this]() {
        addImageFiles(QFileDialog::getOpenFileNames(this, tr("Select images"), QString(),
                                                    tr("Images (*.png *.jpg *.jpeg *.webp *.bmp);;All files (*.*)")));
    });
    connect(m_input, &InputTextEdit::imagesInserted, this,
            [this](const QStringList& files, const QStringList& base64, const QStringList& names) {
        addImageFiles(files);
        addImagesFromBase64(names, base64);
    });
    connect(m_input, &InputTextEdit::signal_textEdited, this, &ChatPanel::requestEdited);
    connect(m_templateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        notifyTemplateSelection();
    });
    connect(m_modelCombo, &QComboBox::currentTextChanged, this, &ChatPanel::onModelChosen);
    // Speech recognition starts on the first click: it launches a Python server and loads a model.
    connect(m_micBtn, &StyledButton::clicked, this, &ChatPanel::onMicClicked);

    // Asking the server for its models must not delay the window.
    QTimer::singleShot(0, this, &ChatPanel::refreshModels);
}

// ---------------------------------------------------------------- templates and models

void ChatPanel::refreshTemplates()
{
    const QString current = m_templateCombo->currentData().toString();
    const QSignalBlocker blocker(m_templateCombo);
    m_templateCombo->clear();
    m_templateCombo->addItem(tr("Choose a default template"), QString());
    m_templateCombo->addItem(tr("Simple chat"), QLatin1String(kSimpleChat));
    m_templateCombo->setItemData(1, QColor(255, 255, 200), Qt::BackgroundRole);
    m_templates.refresh();
    for (const QString& dir : m_templates.templateDirs())
        m_templateCombo->addItem(QFileInfo(dir).fileName(), dir);
    m_templateCombo->loadState();
    if (!current.isEmpty() && m_templateCombo->currentData().toString().isEmpty()) {
        const int index = m_templateCombo->findData(current);
        if (index >= 0)
            m_templateCombo->setCurrentIndex(index);
    }
    notifyTemplateSelection();
}

void ChatPanel::notifyTemplateSelection()
{
    QString path = m_templateCombo->currentData().toString();
    if (path == QLatin1String(kSimpleChat))
        path.clear();
    emit templateSelectionChanged(path);
}

void ChatPanel::refreshModels()
{
    AppConfig cfg;
    cfg.load();
    QString preferred = (cfg.provider.trimmed().isEmpty() || cfg.provider == m_model.providerName())
            ? cfg.selectedModel.trimmed() : QString();
    if (preferred.isEmpty())
        preferred = m_model.model().trimmed();
    const QString endpoint = m_model.endpoint().toString();

    m_model.fetchModels(this, [this, preferred, endpoint](const QStringList& models, const QString& error) {
        m_fillingModels = true;
        m_modelCombo->setItems(models);
        int index = preferred.isEmpty() ? -1 : m_modelCombo->findText(preferred, Qt::MatchExactly);
        if (index < 0 && m_modelCombo->count() > 0)
            index = 0;
        m_modelCombo->setCurrentIndex(index);
        m_fillingModels = false;
        onModelChosen(m_modelCombo->currentText());

        if (models.isEmpty()) {
            // Said where the user looks, not only in the log.
            appendNotice(QStringLiteral("<b>") + tr("No models found at %1.").arg(endpoint.toHtmlEscaped())
                         + QStringLiteral("</b><br>")
                         + tr("Start Ollama and download a model, for example <code>ollama pull qwen2.5-coder:7b</code>, "
                              "or choose another server in Settings &rarr; Model. Then press the refresh button next to the model list.")
                         + (error.isEmpty() ? QString() : QStringLiteral("<br><i>%1</i>").arg(error.toHtmlEscaped())));
        }
    });
}

void ChatPanel::onModelChosen(const QString& model)
{
    if (m_fillingModels)
        return;
    AppConfig cfg;
    cfg.load();
    m_model.configure(model.trimmed(), cfg.generationParams());
    cfg.selectedModel = model.trimmed();
    cfg.provider = m_model.providerName();
    cfg.params = cfg.generationParams();
    cfg.save();
}

// ---------------------------------------------------------------- sending

void ChatPanel::onSend()
{
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty())
        return;
    const QString data = m_templateCombo->currentData().toString();
    Request request;
    request.mode = data.isEmpty() ? Request::Mode::PickTemplate
                 : data == QLatin1String(kSimpleChat) ? Request::Mode::Chat
                                                      : Request::Mode::Template;
    if (request.mode == Request::Mode::Template)
        request.templatePath = data;
    request.text = text;
    m_input->clear();

    describeImages([this, request]() mutable {
        request.textWithImages = textWithImages(request.text);
        for (const ImageAttachment& a : m_images) {
            request.images.paths << a.filePath;
            request.images.base64 << a.base64;
            request.images.summaries << a.summary;
        }
        m_images.clear();
        rebuildImagesBar();
        emit requestSent(request);
    });
}

bool ChatPanel::modelReadsImages() const
{
    static const QStringList vision = {
        QStringLiteral("llava"), QStringLiteral("vision"), QStringLiteral("minicpm-v"), QStringLiteral("qwen2-vl"),
        QStringLiteral("qwen2.5-vl"), QStringLiteral("qwen3-vl"), QStringLiteral("pixtral"), QStringLiteral("moondream"),
        QStringLiteral("bakllava"), QStringLiteral("-vl"), QStringLiteral(":vl"), QStringLiteral("vl-")
    };
    const QString model = m_model.model().toLower();
    for (const QString& v : vision)
        if (model.contains(v))
            return true;
    return false;
}

void ChatPanel::describeImages(std::function<void()> then)
{
    bool missing = false;
    for (const ImageAttachment& a : m_images)
        missing = missing || a.summary.trimmed().isEmpty();
    if (!missing || !modelReadsImages()) {
        then();
        return;
    }

    ModelRequest request;
    request.user = QStringLiteral("Describe each image. Give 1-2 sentences per image.\nStrict format:\n");
    for (int i = 1; i <= m_images.size(); ++i) {
        request.user += QStringLiteral("%1) ...\n").arg(i);
        request.imagesBase64 << m_images.at(i - 1).base64;
    }
    request.options = {{QStringLiteral("temperature"), 0.2}, {QStringLiteral("num_predict"), 256}};
    request.timeoutMs = 30000;
    appendNotice(QStringLiteral("<i>%1</i>").arg(tr("Describing the attached images...")));

    if (m_imageReply)
        m_imageReply->abort();
    ModelReply* reply = m_model.send(request);
    m_imageReply = reply;
    connect(reply, &ModelReply::finished, this, [this, reply, then]() {
        reply->deleteLater();
        const QString answer = reply->hasError() ? QString() : reply->text().trimmed();
        // "1) a cat on a sofa" -> image 1; otherwise the whole answer for every image.
        QMap<int, QString> parsed;
        const QRegularExpression re(QStringLiteral(R"(^\s*(\d+)\s*[\).:-]\s*(.+)\s*$)"));
        for (const QString& line : answer.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QRegularExpressionMatch m = re.match(line);
            if (m.hasMatch())
                parsed[m.captured(1).toInt()] = m.captured(2).trimmed();
        }
        for (int i = 0; i < m_images.size(); ++i) {
            const QString s = parsed.isEmpty() ? answer : parsed.value(i + 1);
            if (!s.isEmpty())
                m_images[i].summary = s;
        }
        if (!reply->isCancelled())
            then();
    });
}

QString ChatPanel::textWithImages(const QString& text) const
{
    if (m_images.isEmpty())
        return text;
    QString out = text + QStringLiteral("\n\n[Images]\n");
    for (const ImageAttachment& a : m_images) {
        out += a.summary.trimmed().isEmpty() ? QStringLiteral("- %1\n").arg(a.fileName)
                                             : QStringLiteral("- %1: %2\n").arg(a.fileName, a.summary.trimmed());
    }
    return out.trimmed();
}

// ---------------------------------------------------------------- the chat log

void ChatPanel::appendUserMessage(const QString& text)
{
    m_chat->append(tr("You: %1").arg(text) + QStringLiteral("\n\n"));
}

void ChatPanel::appendAssistantChunk(const QString& text)
{
    if (!m_assistantLineOpen) {
        m_chat->append(tr("Assistant:") + QStringLiteral(" \n"));
        m_assistantLineOpen = true;
    }
    m_chat->moveCursor(QTextCursor::End);
    m_chat->insertPlainText(text);
}

void ChatPanel::finishAssistantMessage()
{
    if (!m_assistantLineOpen)
        return;
    m_assistantLineOpen = false;
    m_chat->append(QString());
}

void ChatPanel::appendNotice(const QString& html)
{
    setChatLogVisible(true);
    m_chat->append(html);
}

void ChatPanel::setChatLogVisible(bool visible)
{
    if (m_chat->isVisible() == visible)
        return;
    m_chat->setVisible(visible);
    emit chatLogVisibilityChanged(visible);
}

bool ChatPanel::isChatLogVisible() const
{
    return m_chat->isVisible();
}

// ---------------------------------------------------------------- images

void ChatPanel::addImageFiles(const QStringList& files)
{
    for (const QString& path : files) {
        QFile f(path);
        if (!QFileInfo(path).isFile() || !f.open(QIODevice::ReadOnly))
            continue;
        const QByteArray bytes = f.readAll();
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QImage img = reader.read();
        if (img.isNull())
            img = QImage::fromData(bytes);
        if (img.isNull())
            continue;
        ImageAttachment att;
        att.filePath = QDir::toNativeSeparators(path);
        att.fileName = QFileInfo(path).fileName();
        att.base64 = QString::fromLatin1(bytes.toBase64());
        att.thumb = QPixmap::fromImage(img.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_images.push_back(att);
    }
    rebuildImagesBar();
}

void ChatPanel::addImagesFromBase64(const QStringList& names, const QStringList& base64)
{
    for (int i = 0; i < base64.size(); ++i) {
        const QString b64 = base64.at(i).trimmed();
        const QImage img = QImage::fromData(QByteArray::fromBase64(b64.toLatin1()));
        if (b64.isEmpty() || img.isNull())
            continue;
        const QString name = (i < names.size() && !names.at(i).trimmed().isEmpty())
                ? names.at(i).trimmed() : QStringLiteral("clipboard_image_%1.png").arg(i + 1);
        ImageAttachment att;
        att.fileName = name;
        att.filePath = QStringLiteral("clipboard:%1").arg(name);
        att.base64 = b64;
        att.thumb = QPixmap::fromImage(img.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_images.push_back(att);
    }
    rebuildImagesBar();
}

void ChatPanel::rebuildImagesBar()
{
    while (QLayoutItem* item = m_imagesLayout->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    for (int i = 0; i < m_images.size(); ++i) {
        auto* cell = new QWidget(m_imagesBar);
        cell->setFixedSize(86, 86);
        auto* grid = new QGridLayout(cell);
        grid->setContentsMargins(0, 0, 0, 0);
        auto* picture = new QLabel(cell);
        picture->setPixmap(m_images.at(i).thumb);
        picture->setAlignment(Qt::AlignCenter);
        picture->setToolTip(m_images.at(i).filePath);
        picture->setFixedSize(86, 86);
        picture->setStyleSheet(QStringLiteral("border: 1px solid #4a4a4a; border-radius: 6px; background: #222;"));
        grid->addWidget(picture, 0, 0, Qt::AlignCenter);
        auto* remove = new QToolButton(cell);
        remove->setText(QStringLiteral("x"));
        remove->setToolTip(tr("Remove"));
        remove->setFixedSize(18, 18);
        remove->setStyleSheet(QStringLiteral(
                "QToolButton { background: rgba(0,0,0,180); color: white; border: 1px solid #666; border-radius: 9px; }"
                "QToolButton:hover { border-color: #aaa; }"));
        grid->addWidget(remove, 0, 0, Qt::AlignTop | Qt::AlignRight);
        connect(remove, &QToolButton::clicked, this, [this, i]() {
            if (i < m_images.size()) {
                m_images.removeAt(i);
                rebuildImagesBar();
            }
        });
        m_imagesLayout->addWidget(cell);
    }
    m_imagesLayout->addStretch(1);
    m_imagesScroll->setVisible(!m_images.isEmpty());
}

// ---------------------------------------------------------------- voice

void ChatPanel::ensureSpeechRecognizer()
{
    const SpeechRecognizer::Engine engine = SpeechRecognizer::configuredEngine();
    if (m_recognizer && m_recognizerEngine == int(engine))
        return;
    if (m_recognizer) {
        if (m_recognizer->isRecording())
            return;  // the engine was changed in Settings: replace it once recording stops
        m_recognizer->deleteLater();
    }
    m_recognizer = SpeechRecognizer::create(engine, this);
    m_recognizerEngine = int(engine);
    connect(m_recognizer, &SpeechRecognizer::recognitionFinished, this, &ChatPanel::onAsrFinished);
    connect(m_recognizer, &SpeechRecognizer::errorOccurred, this, &ChatPanel::onAsrError);
    connect(m_recognizer, &SpeechRecognizer::silenceDetected, this, [this]() {
        if (!m_input->toPlainText().trimmed().isEmpty())
            onSend();  // a pause after speaking sends the message
    });
    connect(m_recognizer, &SpeechRecognizer::statusMessage, this, [this](const QString& message) {
        appendNotice(QStringLiteral("<i>%1</i>").arg(message.toHtmlEscaped()));
    });
}

void ChatPanel::setRecording(bool recording)
{
    m_isRecording = recording;
    m_micBtn->setText(recording ? QStringLiteral("🎙️") : QStringLiteral("🎤"));
    m_micBtn->setToolTip(recording ? tr("Recording... (click to stop)") : tr("Voice input (click to record)"));
    m_micBtn->setStyleSheet(QLatin1String(recording ? kMicRecordingStyle : kMicIdleStyle));
}

void ChatPanel::onMicClicked()
{
    ensureSpeechRecognizer();
    if (m_isRecording) {
        m_recognizer->stopRecording();
        setRecording(false);
    } else {
        m_recognizer->startRecording();
        setRecording(true);
    }
}

void ChatPanel::onAsrFinished(const QString& text)
{
    const QString spoken = normalizeSpeech(text);
    if (spoken.isEmpty())
        return;
    m_input->insertPlainText(spoken + QLatin1Char(' '));
    m_input->setFocus();

    // A trigger word at the end sends the message (and is removed from it).
    QString lower = spoken.toLower();
    lower.remove(QRegularExpression(QStringLiteral(R"([.!?,;:]+$)")));
    for (const QString& trigger : sendTriggers()) {
        if (!lower.endsWith(trigger))
            continue;
        QString input = m_input->toPlainText().trimmed();
        input.remove(QRegularExpression(QStringLiteral(R"([.!?,;:]+$)")));
        if (input.toLower().endsWith(trigger))
            input.chop(trigger.length());
        m_input->setPlainText(input.trimmed());
        onSend();
        return;
    }
}

void ChatPanel::onAsrError(const QString& message)
{
    setRecording(false);
    appendNotice(tr("Speech recognition error: %1").arg(message).toHtmlEscaped());
}
