#pragma once

#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QTreeWidget>
#include <QTabWidget>
#include <QStackedWidget>
#include <QSplitter>

// Forward declarations
class IDEWidget;
class MemoryLineEdit;
class StyleTextEdit;
class StyledButton;
class MemoryComboBox;
class MemorySplitter;
class ChatDialog;
class GraphWidget;
class SequenceGraphWidget;
class ResultDisplayWidget;
class VoskWidget;
class ContextGraphWidget;
class OllamaSettingsDialog;

/*
 * Holds all UI widget pointers for MainWindow.
 * Passed to UiBuilder for population — avoids friend-class access.
 */
struct UIWidgets {
    // Main widgets
    IDEWidget* ide_widget = nullptr;
    MemoryLineEdit* m_rootEdit = nullptr;
    GraphWidget* m_graphWindow = nullptr;
    SequenceGraphWidget* m_seqGraphWindow = nullptr;
    StyledButton* browseBtn = nullptr;

    // File list
    QListWidget* m_files = nullptr;

    // Log & Chat
    StyleTextEdit* m_log = nullptr;
    ChatDialog* m_chat = nullptr;

    // Result display
    ResultDisplayWidget* m_resultDisplayWidget = nullptr;

    // Vosk
    VoskWidget* m_voskWidget = nullptr;

    // Context status
    QWidget* m_contextStatusWidget = nullptr;
    QTreeWidget* m_contextCommandsList = nullptr;
    ContextGraphWidget* m_contextGraphWidget = nullptr;
    QTabWidget* m_contextTabs = nullptr;

    // Settings
    QWidget* m_settingsPanel = nullptr;
    QStackedWidget* m_settingsPages = nullptr;  // pages stack
    QListWidget* m_settingsSidebar = nullptr;    // sidebar

    // Buttons
    StyledButton* applyBtn = nullptr;
    StyledButton* undoBtn = nullptr;
    StyledButton* runSeqBtn = nullptr;
    StyledButton* rescanBtn = nullptr;
    StyledButton* descBtn = nullptr;
    StyledButton* settingsBtn = nullptr;
    StyledButton* promptBtn = nullptr;
    StyledButton* editSeqBtn = nullptr;
    StyledButton* newSeqBtn = nullptr;
    StyledButton* runTemplateBtn = nullptr;
    StyledButton* editTemplatesBtn = nullptr;
    StyledButton* exportSequenceAsTemplateBtn = nullptr;
    StyledButton* graphBtn = nullptr;
    StyledButton* seqGraphBtn = nullptr;
    StyledButton* chatToggleBtn = nullptr;
    StyledButton* logToggleBtn = nullptr;
    StyledButton* lightRagBtn = nullptr;

    // Combo boxes
    MemoryComboBox* confirmModeCombo = nullptr;

    // Splitters
    MemorySplitter* m_centerSplitter = nullptr;
    MemorySplitter* mainSplitter = nullptr;

    // Settings dialog
    OllamaSettingsDialog* m_llmSettingsWindow = nullptr;

    // Top widget (path + browse)
    QWidget* top_widget = nullptr;
};
