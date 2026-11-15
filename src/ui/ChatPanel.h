#pragma once
#include <QPixmap>
#include <QPointer>
#include <QVector>
#include <QWidget>
#include <functional>
#include "core/session/Conversation.h"

class InputTextEdit;
class LlmClient;
class MemoryComboBox;
class ModelReply;
class QHBoxLayout;
class QScrollArea;
class SolutionTemplateManager;
class SpeechRecognizer;
class StyleTextEdit;
class StyledButton;

// The chat at the bottom of the main window: the request input with attached images and
// voice input, the choice of template and model, and the streamed answers. It knows nothing
// about how a request is carried out: it emits requestSent() and shows what it is given.
class ChatPanel : public QWidget
{
    Q_OBJECT
public:
    struct Request
    {
        enum class Mode {
            PickTemplate,  // let the user choose a template or sequence first
            Template,      // run `templatePath`
            Chat           // a plain chat message, no commands
        };
        Mode mode = Mode::PickTemplate;
        QString templatePath;
        QString text;            // as typed
        QString textWithImages;  // with a line per attached image (and its description)
        Conversation::Images images;
    };

    ChatPanel(LlmClient& model, SolutionTemplateManager& templates, QWidget* parent = nullptr);

    // Asks the model server for its models and selects the saved one.
    void refreshModels();
    void refreshTemplates();

    void appendUserMessage(const QString& text);
    void appendAssistantChunk(const QString& text);
    void finishAssistantMessage();
    void appendNotice(const QString& html);

    void setChatLogVisible(bool visible);
    bool isChatLogVisible() const;

signals:
    void requestSent(const ChatPanel::Request& request);
    void stopRequested();
    // The text in the input changed (typed or dictated).
    void requestEdited(const QString& text);
    // The template chosen in the list; empty for "choose" and for the plain chat.
    void templateSelectionChanged(const QString& templatePath);
    void chatLogVisibilityChanged(bool visible);

private:
    struct ImageAttachment
    {
        QString filePath;
        QString fileName;
        QString base64;
        QString summary;
        QPixmap thumb;
    };

    void onSend();
    void onModelChosen(const QString& model);
    void onMicClicked();
    void onAsrFinished(const QString& text);
    void onAsrError(const QString& message);
    void ensureSpeechRecognizer();
    void setRecording(bool recording);
    void addImageFiles(const QStringList& files);
    void addImagesFromBase64(const QStringList& names, const QStringList& base64);
    void rebuildImagesBar();
    bool modelReadsImages() const;
    // Asks a vision model to describe the attached images, then calls `then`.
    void describeImages(std::function<void()> then);
    QString textWithImages(const QString& text) const;
    void notifyTemplateSelection();

    LlmClient& m_model;
    SolutionTemplateManager& m_templates;

    StyleTextEdit* m_chat = nullptr;
    InputTextEdit* m_input = nullptr;
    StyledButton* m_send = nullptr;
    StyledButton* m_stop = nullptr;
    StyledButton* m_attachBtn = nullptr;
    StyledButton* m_micBtn = nullptr;
    MemoryComboBox* m_modelCombo = nullptr;
    MemoryComboBox* m_templateCombo = nullptr;
    bool m_assistantLineOpen = false;
    bool m_fillingModels = false;

    SpeechRecognizer* m_recognizer = nullptr;  // created on first use, for the engine chosen in Settings
    int m_recognizerEngine = -1;
    bool m_isRecording = false;

    QScrollArea* m_imagesScroll = nullptr;
    QWidget* m_imagesBar = nullptr;
    QHBoxLayout* m_imagesLayout = nullptr;
    QVector<ImageAttachment> m_images;
    QPointer<ModelReply> m_imageReply;
};
