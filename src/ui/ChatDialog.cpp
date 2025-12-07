#include "ChatDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QGridLayout>
#include <QDirIterator>
#include <QStandardPaths>
#include <QFileInfo>
#include <QFile>
#include "../core/SolutionTemplateManager.h"
#include "../core/SolutionTemplate.h"
#include "MainWindow.h"

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
    m_micBtn->setFixedWidth(30);
    
    // Add only the input field to the main horizontal layout
    mainLayout->addWidget(m_input, 1);
    
    // Create a vertical layout for ALL controls to the right of input
    auto rightColumnLayout = new QVBoxLayout();
    
    // Create a horizontal layout for the send/stop buttons
    auto buttonsLayout = new QHBoxLayout();
    buttonsLayout->addWidget(m_send);
    buttonsLayout->addWidget(m_stop);
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

    connect(m_cb_model, &MemoryComboBox::currentTextChanged, this, [this](const QString &text){
        m_model->configure(text, QVariantMap());
    });

    pslot_updateModelsList();

    connect(m_input,  &InputTextEdit::signal_textEdited, this, [this](const QString &str){

        emit signal_updatelastUserMessage(str);
    });

    m_whisperRunner = new FasterWhisperRunner(this);
    
    connect(m_micBtn, &StyledButton::clicked, this, &ChatDialog::onMicClicked);
    
    connect(m_whisperRunner, &FasterWhisperRunner::recognitionFinished, this, &ChatDialog::onAsrFinished);
    connect(m_whisperRunner, &FasterWhisperRunner::errorOccurred, this, &ChatDialog::onAsrError);
    connect(m_whisperRunner, &FasterWhisperRunner::statusMessage, this, [this](const QString& msg){
        if (m_chat) {
            m_chat->setVisible(true); // Ensure chat is visible
            m_chat->append("<i>" + msg + "</i>");
        }
    });
}

void ChatDialog::onMicClicked() {
    if (m_isRecording) {
        m_whisperRunner->stopRecording();
        
        m_micBtn->setText("...");
        m_micBtn->setEnabled(false);
        m_isRecording = false;
        
        // Hide chat log if it was only showing status
        if (m_chat && m_chat->toPlainText().contains("Faster Whisper")) {
            m_chat->setVisible(false);
            m_chat->clear();
        }
    } else {
        // Use Faster Whisper by default for streaming experience
        m_whisperRunner->startRecording();
        
        m_micBtn->setText("⏹");
        m_micBtn->setStyleSheet("background-color: red; color: white;");
        m_isRecording = true;
    }
}

void ChatDialog::onAsrFinished(const QString& text) {
    // Keep button active for streaming
    // m_micBtn->setText("🎤");
    // m_micBtn->setEnabled(true);
    // m_micBtn->setStyleSheet(""); // Reset style
    // m_isRecording = false;
    
    if (!text.isEmpty()) {
        setInputText(text);
    }
}

void ChatDialog::onAsrError(const QString& msg) {
    m_micBtn->setText("🎤");
    m_micBtn->setEnabled(true);
    m_micBtn->setStyleSheet(""); // Reset style
    m_isRecording = false;
    // Show error in chat or log
    m_chat->append("ASR Error: " + msg);
}

void ChatDialog::onStop(){
    if (m_model) m_model->stopStream();
    // Also signal the MainWindow to stop template execution if running
    if (m_mainWnd) {
        m_mainWnd->stopTemplateExecution();
    }
}

void ChatDialog::pslot_updateModelsList(){
    //QString current_text = m_cb_model->currentText();
    //qDebug()<<"current_text = "<<current_text;
    m_cb_model->setItems(m_model->listModels());


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
    
    // Get the current selection from the template combobox
    QString selectedTemplateData = m_templateCombo->currentData().toString();
    QString selectedTemplateName = m_templateCombo->currentText();
    
    if (selectedTemplateData == "simple_chat") {
        // Direct chat mode - send directly to model in streaming mode
        // We'll let MainWindow handle displaying the message in the chat
        m_input->clear();
        
        // Emit signal for simple chat mode
        emit runSimpleChat(t);
        return;
    }
    else if (selectedTemplateData.isEmpty()) {
        // Default behavior - open template/sequence selection dialog
        // We'll let MainWindow handle displaying the message in the chat
        m_input->clear();
        emit sendMessage(t);
        return;
    }
    else {
        // A specific template was selected - emit signal to run this template
        // We'll let MainWindow handle displaying the message in the chat
        m_input->clear();
        
        // Emit signal to run the selected template
        emit runTemplateRequested(selectedTemplateData, t);
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
    
    // Restore the previous selection if it still exists, otherwise keep default
    if (!currentSelection.isEmpty()) {
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
