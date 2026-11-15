#pragma once
#include <QDialog>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <StyledButton.h>
#include "../adapters/OllamaAdapter.h"
#include <QComboBox>
#include <QLabel>
#include <QKeyEvent>
#include <QTimer>
#include <InputTextEdit.h>
#include <MemoryComboBox.h>
#include <StyleTextEdit.h>
#include "../audio/FasterWhisperRunner.h"
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVector>

class ChatDialog : public QWidget {
    Q_OBJECT
public:
    explicit ChatDialog(OllamaAdapter* model, QWidget* parent = nullptr);
    void refreshModelList();

    void appendAssistantChunk(const QString& s);
    void appendUserMessage(const QString& s);
    void finishAssistantMessage();
    void sendMessageToChat(const QString& text);
    void setInputText(const QString& text);
    void setTemplateManager(class SolutionTemplateManager* templateManager);
    void setMainWindow(class MainWindow* mainWnd);
    void setChatLogVisible(bool visible);
    bool isChatLogVisible() const;
    bool isRecording() const { return m_isRecording; }
    void resumeRecordingAfterResponse();


signals:
    void sendMessage(const QString& text);
    void sendMessageWithImages(const QString& text, const QStringList& imagePaths, const QStringList& imagesBase64, const QStringList& imageSummaries);
    void signal_updatelastUserMessage(const QString& lastUserMessage);
    void runTemplateRequested(const QString& templatePath, const QString& text);
    void runTemplateRequestedWithImages(const QString& templatePath, const QString& text, const QStringList& imagePaths, const QStringList& imagesBase64, const QStringList& imageSummaries);
    void runSimpleChat(const QString& text);
    void runSimpleChatWithImages(const QString& text, const QStringList& imagePaths, const QStringList& imagesBase64, const QStringList& imageSummaries);
    void templateSelectionChanged(const QString& templatePath);

private slots:
    void onSend();
    void onStop();
    void pslot_updateModelsList();
    void onMicClicked();
    void onAttachImages();
    void onImagesInserted(const QStringList &filePaths,
                          const QStringList &imagesBase64Png,
                          const QStringList &imageNames);
    void onAsrFinished(const QString& text);
    void onAsrError(const QString& msg);
    void onSilenceDetected();

private:
    void populateTemplates();
    void notifyTemplateSelectionChanged();
    bool checkTriggerPhrase(const QString& text);

    struct ImageAttachment {
        QString filePath;
        QString fileName;
        QString base64;
        QString summary;
        QPixmap thumb;
    };

    void rebuildImagesBar();
    void clearImageAttachments();
    bool modelSupportsVision() const;
    QStringList ensureImageSummaries(QString* combinedOut = nullptr);
    QString buildAugmentedTextWithImages(const QString& text, const QVector<ImageAttachment>& attachments) const;
    void addImageAttachmentsFromFiles(const QStringList &files);
    void addImageAttachmentsFromBase64(const QStringList &imageNames, const QStringList &imagesBase64);
    
    StyleTextEdit* m_chat = nullptr;
    InputTextEdit* m_input = nullptr;
    StyledButton* m_send = nullptr;
    StyledButton* m_stop = nullptr;
    StyledButton* m_attachBtn = nullptr;
    OllamaAdapter* m_model = nullptr;
    MemoryComboBox * m_cb_model;
    MemoryComboBox * m_templateCombo;
    class SolutionTemplateManager* m_templateManager = nullptr;
    class MainWindow* m_mainWnd = nullptr;
    StyledButton* m_micBtn = nullptr;
    FasterWhisperRunner* m_whisperRunner = nullptr; // created on first use
    void ensureSpeechRecognizer();
    bool m_isRecording = false;

    QScrollArea* m_imagesScroll = nullptr;
    QWidget* m_imagesBar = nullptr;
    QHBoxLayout* m_imagesLayout = nullptr;
    QVector<ImageAttachment> m_imageAttachments;
};
