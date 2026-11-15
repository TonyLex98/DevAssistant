#pragma once

#include "ui/controllers/UIWidgets.h"
#include <QString>
#include <functional>
#include <QVBoxLayout>

class OllamaAdapter;
class PromptSequenceManager;
class SolutionTemplateManager;
class TtsController;

/*
 * UiBuilder — static helper that constructs all widgets/layouts for MainWindow.
 *
 * Takes a UIWidgets struct (by reference) and populates it with created widgets.
 * Dependencies are passed explicitly — no MainWindow* or friend-class access.
 */
class UiBuilder {
public:
    // Main entry point — builds all widgets and layouts
    // ui: struct to populate with widget pointers
    // tr_fn: translation function (the "UiBuilder" translation context)
    // model: OllamaAdapter for ChatDialog
    // seqMgr: PromptSequenceManager for SequenceGraphWidget
    // tmplMgr: SolutionTemplateManager for SequenceGraphWidget
    // tts: TtsController for audio page
    static void build(UIWidgets& ui,
                      std::function<QString(const char*)> tr_fn,
                      OllamaAdapter* model,
                      PromptSequenceManager* seqMgr,
                      SolutionTemplateManager* tmplMgr,
                      TtsController* tts,
                      QWidget* parent);

private:
    // Helpers for settings pages
    static void buildSettingsPanel(UIWidgets& ui,
                                   std::function<QString(const char*)> tr_fn,
                                   TtsController* tts,
                                   QWidget* parent);
    static QWidget* makePage();
    static QVBoxLayout* pageLay(QWidget* w);
    static void addSection(const QString& title, QVBoxLayout* layout);
    static void addDiv(QVBoxLayout* layout);
    static void buildProjectPage(UIWidgets& ui,
                                 std::function<QString(const char*)> tr_fn,
                                 QWidget* page, QWidget* parent);
    static void buildModelPage(UIWidgets& ui,
                               std::function<QString(const char*)> tr_fn,
                               QWidget* page, QWidget* parent);
    static void buildSequencesPage(UIWidgets& ui,
                                   std::function<QString(const char*)> tr_fn,
                                   QWidget* page, QWidget* parent);
    static void buildGraphsPage(UIWidgets& ui,
                                std::function<QString(const char*)> tr_fn,
                                QWidget* page, QWidget* parent);
    static void buildAudioPage(std::function<QString(const char*)> tr_fn,
                               TtsController* tts, QWidget* page);
    static void buildSystemPage(UIWidgets& ui,
                                std::function<QString(const char*)> tr_fn,
                                QWidget* page, QWidget* parent);

    // Layout assembly
    static void assembleLayout(UIWidgets& ui, QWidget* parent);
};
