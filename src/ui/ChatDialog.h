#pragma once
#include <QDialog>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <StyledButton.h>
#include "../adapters/OllamaAdapter.h"
#include <QComboBox>
#include <QLabel>
#include <QKeyEvent>
#include <InputTextEdit.h>
#include <MemoryComboBox.h>
#include <StyleTextEdit.h>

class ChatDialog : public QWidget {
    Q_OBJECT
public:
    explicit ChatDialog(OllamaAdapter* model, QWidget* parent = nullptr);

    void appendAssistantChunk(const QString& s);
    void appendUserMessage(const QString& s);
    void finishAssistantMessage();
    void sendMessageToChat(const QString& text);
    void setTemplateManager(class SolutionTemplateManager* templateManager);
    void setMainWindow(class MainWindow* mainWnd);

signals:
    void sendMessage(const QString& text);
    void signal_updatelastUserMessage(const QString& lastUserMessage);
    void runTemplateRequested(const QString& templatePath, const QString& text);
    void runSimpleChat(const QString& text);

private slots:
    void onSend();
    void onStop();
    void pslot_updateModelsList();

private:
    void populateTemplates();
    StyleTextEdit* m_chat = nullptr;
    InputTextEdit* m_input = nullptr;
    StyledButton* m_send = nullptr;
    StyledButton* m_stop = nullptr;
    OllamaAdapter* m_model = nullptr;
    MemoryComboBox * m_cb_model;
    MemoryComboBox * m_templateCombo;
    class SolutionTemplateManager* m_templateManager = nullptr;
    class MainWindow* m_mainWnd = nullptr;
};
