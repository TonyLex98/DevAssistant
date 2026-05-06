#pragma once

#include <QWidget>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QComboBox>
#include <QVBoxLayout>

class StyledButton;
class MemoryComboBox;
class OllamaSettingsDialog;
class TtsController;

/*
 * SettingsPageBuilder — строит 6 страниц настроек в QStackedWidget
 * 
 * Страницы:
 *  0 — Проект (путь, rescan, generate descriptions)
 *  1 — Модель (LLM settings, prompt editor)
 *  2 — Последовательности (edit, new, run, export)
 *  3 — Графики (context graph, sequence graph, LightRAG)
 *  4 — Аудио (TTS toggle)
 *  5 — Система (confirm mode, chat/log toggle)
 */
class SettingsPageBuilder
{
public:
    // Returns the sidebar list widget (caller owns it)
    QListWidget* sidebar() const { return m_sidebar; }
    // Returns the stacked widget containing all pages
    QStackedWidget* pages() const { return m_pages; }

    // Created buttons/pages — accessed by MainWindow for connections
    StyledButton* rescanBtn() const { return m_rescanBtn; }
    StyledButton* descBtn() const { return m_descBtn; }
    StyledButton* settingsBtn() const { return m_settingsBtn; }
    StyledButton* promptBtn() const { return m_promptBtn; }
    StyledButton* editSeqBtn() const { return m_editSeqBtn; }
    StyledButton* newSeqBtn() const { return m_newSeqBtn; }
    StyledButton* runTemplateBtn() const { return m_runTemplateBtn; }
    StyledButton* editTemplatesBtn() const { return m_editTemplatesBtn; }
    StyledButton* exportSequenceAsTemplateBtn() const { return m_exportSequenceAsTemplateBtn; }
    StyledButton* graphBtn() const { return m_graphBtn; }
    StyledButton* seqGraphBtn() const { return m_seqGraphBtn; }
    StyledButton* lightRagBtn() const { return m_lightRagBtn; }
    StyledButton* chatToggleBtn() const { return m_chatToggleBtn; }
    StyledButton* logToggleBtn() const { return m_logToggleBtn; }
    MemoryComboBox* confirmModeCombo() const { return m_confirmModeCombo; }
    OllamaSettingsDialog* llmSettingsWindow() const { return m_llmSettingsWindow; }

    /*
     * Build all 6 settings pages.
     *
     * Parameters:
     *   parent            — QWidget parent for all created widgets
     *   projectPathWidget — the top_widget (project path row) from MainWindow
     *   ttsController     — TtsController instance for audio page
     *   llmSettingsParent — parent for OllamaSettingsDialog
     */
    void buildPages(QWidget* parent,
                    QWidget* projectPathWidget,
                    TtsController* ttsController,
                    QWidget* llmSettingsParent);

private:
    // Helpers
    QWidget* makePage();
    QVBoxLayout* pageLay(QWidget* w);
    void addSection(const QString& title, QVBoxLayout* layout);
    void addDiv(QVBoxLayout* layout);
    void setAllButtonsExpanding();

    // Widgets
    QListWidget*      m_sidebar = nullptr;
    QStackedWidget*   m_pages   = nullptr;

    // Page 0: Project
    StyledButton* m_rescanBtn = nullptr;
    StyledButton* m_descBtn = nullptr;

    // Page 1: Model
    StyledButton*      m_settingsBtn = nullptr;
    StyledButton*      m_promptBtn = nullptr;
    OllamaSettingsDialog* m_llmSettingsWindow = nullptr;

    // Page 2: Sequences
    StyledButton* m_editSeqBtn = nullptr;
    StyledButton* m_newSeqBtn = nullptr;
    StyledButton* m_runTemplateBtn = nullptr;
    StyledButton* m_editTemplatesBtn = nullptr;
    StyledButton* m_exportSequenceAsTemplateBtn = nullptr;

    // Page 3: Graphs
    StyledButton* m_graphBtn = nullptr;
    StyledButton* m_seqGraphBtn = nullptr;
    StyledButton* m_lightRagBtn = nullptr;

    // Page 5: System
    MemoryComboBox* m_confirmModeCombo = nullptr;
    StyledButton*   m_chatToggleBtn = nullptr;
    StyledButton*   m_logToggleBtn = nullptr;
};
