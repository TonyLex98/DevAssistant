#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPair>
#include "../core/ProjectIndex.h"
#include "../core/ChangeApplier.h"
#include "../adapters/OllamaAdapter.h"
#include "../utils/PromptManager.h"
#include <QPushButton>
#include <DevAssManager.h>
#include <QProgressDialog>
#include "DescriptionDialog.h"      // ← новый диалог
#include <CommandParser.h>
#include <CommandConfirmDialog.h>
#include <OpenFileContextCommand.h>
#include <QString>
#include "../audio/VoskWidget.h"

class ChatDialog;
class ModelsDialog;
class OllamaSettingsDialog;
class PromptEditorDialog;
class ChangesDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);

private slots:
    void browse();
    void rescan();
    void openSettings();
    void editBasePrompt();
    void applyChanges();
    void undoLast();

    void handleAssistantFinished();

    void generateDescriptions();      // <‑‑ новый слот
    void onVoskText(const QString&);  // <‑‑ новый слот

private:
    QString buildProjectContext() const;
    void log(const QString& m);

    void sendUserMessage(const QString& text);
    void onModelChunk(const QString& s);
    QByteArray extractJsonPart(const QByteArray& txt) const;


private:
    QString m_projectRoot;
    ProjectIndex m_index;
    ChangeApplier m_applier;
    OllamaAdapter m_model;
    PromptManager m_promptMgr;

    QListWidget* m_files=nullptr;
    QLineEdit* m_rootEdit=nullptr;
    QPlainTextEdit* m_log=nullptr;

    ChatDialog* m_chat=nullptr;
    QList<Change> m_pending;

    QList<QPair<QString,QString>> m_history;
    QString m_streamBuf;

    QPushButton* rescanBtn;
    QPushButton* settingsBtn;
    QPushButton* promptBtn;
    QPushButton* applyBtn;
    QPushButton* undoBtn;
    QPushButton* browseBtn;
    DevAssManager m_dev_manager;
    QPushButton* descBtn = nullptr;   // <‑‑ новая кнопка
    VoskWidget* m_voskWidget = nullptr;  // <‑‑ виджет Vosk

    QString m_contextContent;
};