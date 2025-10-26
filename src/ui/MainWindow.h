#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <StyleTextEdit.h>
#include <QLineEdit>
#include <QPair>
#include <runSequenceDialog.h>
#include "../core/ProjectIndex.h"
#include "../core/ChangeApplier.h"
#include "../adapters/OllamaAdapter.h"
#include "../utils/PromptManager.h"
#include <StyledButton.h>
#include <DevAssManager.h>
#include <QProgressDialog>
#include "DescriptionDialog.h"      // ← новый диалог
#include <CommandParser.h>
#include <CommandConfirmDialog.h>
#include <OpenFileContextCommand.h>
#include <QString>
#include "../audio/VoskWidget.h"
#include <PromptSequenceManager.h>
#include <QMessageBox>
#include  <PromptResolver.h>
#include <QTimer>
#include <PromptSequenceEditor.h>
#include <QDialogButtonBox>
#include <QStandardPaths>
#include <IDEWidget.h>
#include <QToolBar>
#include <QDockWidget>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDir>
#include <QStandardPaths>
#include <QTabWidget>
#include <MemoryLineEdit.h>
#include <ProjectRelationView.h>
#include "GraphWidget.h"
#include <QMap>
#include <QVector>

class ChatDialog;
class ModelsDialog;
class OllamaSettingsDialog;
class PromptEditorDialog;
class ChangesDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);

    const QString& projectRootPath() const { return m_projectRoot; }
    PromptManager*  promptManager() { return &m_promptMgr; }
    const QList<QPair<QString,QString>>& history() const { return m_history; }
    const QString& openedFilesContent() const { return m_contextContent; }
    const QString& lastUserMessage() const { return m_lastUserMessage; }
    const QString& lastAssistantMessage() const { return m_lastAssistantMessage; }


public slots:
    void runSequenceDialog();
    void slot_updatelastUserMessage(const QString& lastUserMessage);


signals:

    void missingFilesUpdated(const QSet<QString> &paths);


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

    void openGraph();


public:
    // Accessors used by PromptResolver
    QString previousStepsText() const;
    QString fileDescriptionsText() const;

private slots:
    void createNewSequence();
    //void onFileDoubleClicked(QListWidgetItem *item);

private:
    QString buildProjectContext() const;
    void log(const QString& m);

    void sendUserMessage(const QString& text);
    void onModelChunk(const QString& s);
    QByteArray extractJsonPart(const QByteArray& txt) const;
    void editSequences();


private:
    QString m_projectRoot;
    ProjectIndex m_index;
    ChangeApplier m_applier;
    OllamaAdapter m_model;
    PromptManager m_promptMgr;

    QListWidget* m_files=nullptr;
    MemoryLineEdit* m_rootEdit=nullptr;
    StyleTextEdit* m_log=nullptr;

    ChatDialog* m_chat=nullptr;
    QList<Change> m_pending;

    QList<QPair<QString,QString>> m_history;
    QString m_streamBuf;

    StyledButton* rescanBtn;
    StyledButton* settingsBtn;
    StyledButton* promptBtn;
    StyledButton* applyBtn;
    StyledButton* undoBtn;
    StyledButton* browseBtn;
    StyledButton* runSeqBtn;
    StyledButton *editSeqBtn;
    StyledButton *newSeqBtn;
    DevAssManager m_dev_manager;
    StyledButton* descBtn = nullptr;   // <‑‑ новая кнопка
    VoskWidget* m_voskWidget = nullptr;  // <‑‑ виджет Vosk

    QString m_contextContent;

    QStringList m_prevSteps;

    // ... уже есть
    PromptSequenceManager m_seqMgr;          // менеджер наборов
    PromptSequence        m_currentSeq;     // текущая выбранная последовательность
    bool                  m_sequenceMode = false;
    int                   m_currentStepIdx = -1; // -1 = не запущено


    void runNextStep();
    /* ────── NEW ────── */
    static PromptSequence defaultSequenceTemplate();   // шаблон по умолчанию
    void ensureDefaultSequence();                      // создаём, если нет
    /* ────── END ────── */

    void reOpenRootDir();

    QString m_lastUserMessage;
    QString m_lastAssistantMessage;
    IDEWidget *ide_widget;

    MemorySplitter *mainSplitter;

    QTabWidget *project_view_tabs = new QTabWidget();
    ProjectRelationView *project_view = new ProjectRelationView;

    StyledButton *graphBtn{nullptr};    // кнопка «Show Graph»
    GraphWidget  *m_graphWindow{nullptr};   // окно-виджет графа

    struct ContextRange {
        int startLine = -1;
        int endLine = -1;
        QStringList lines;
    };
    struct FileContextSlice {
        enum Mode { None, FullFile, Ranges } mode = None;
        QStringList fullFileLines;
        QVector<ContextRange> regions;
    };

    QMap<QString, FileContextSlice> m_contextSlices;
    QStringList m_consoleLogs;

    void rebuildContextCache();
    QString formatNumberedLines(const QStringList& lines, int startLine) const;
    void setFullFileContext(const QString& path, const QStringList& lines);
    void addRegionContext(const QString& path, int startLine, const QStringList& lines);
    void clearContextForPath(const QString& path);
    bool applyGroupedEditInRange(const QList<Command*>& commands, QString* err);
    void handleOpenCommand(Command* cmd);
    void handlePostApplyContextCleanup(Command* cmd);
    void appendConsoleContext(const QString& title,
                              const QString& stdOut,
                              const QString& stdErr,
                              int exitCode);
    void retryCurrentSequenceStep();

};
