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
class ChatPanel;
class QCheckBox;
class GraphWidget;
class SequenceGraphWidget;
class ResultDisplayWidget;
class ContextGraphWidget;
class ModelSettingsPanel;

// The widgets of the main window, filled in by UiBuilder.
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
    ChatPanel* m_chat = nullptr;

    // Result display
    ResultDisplayWidget* m_resultDisplayWidget = nullptr;

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
    StyledButton* undoBtn = nullptr;
    StyledButton* runSeqBtn = nullptr;
    StyledButton* rescanBtn = nullptr;
    StyledButton* descBtn = nullptr;
    StyledButton* settingsBtn = nullptr;
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
    StyledButton* documentsBtn = nullptr;

    // Combo boxes
    MemoryComboBox* confirmModeCombo = nullptr;
    QCheckBox* agentModeCheck = nullptr;

    // Splitters
    MemorySplitter* m_centerSplitter = nullptr;
    MemorySplitter* mainSplitter = nullptr;

    // Settings dialog
    ModelSettingsPanel* m_llmSettingsWindow = nullptr;

    // Top widget (path + browse)
    QWidget* top_widget = nullptr;
};
