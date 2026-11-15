#pragma once

#include "ui/controllers/UIWidgets.h"
#include <QString>
#include <functional>
#include <QVBoxLayout>

class Session;
class TtsController;

// Builds the widgets and layouts of the main window into a UIWidgets struct, the way a
// Qt Designer form would; the window then connects them to the session and the agent.
class UiBuilder {
public:
    // tr_fn translates in the "UiBuilder" context (lupdate: -tr-function-alias tr+=tr_fn).
    static void build(UIWidgets& ui,
                      std::function<QString(const char*)> tr_fn,
                      Session& session,
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
