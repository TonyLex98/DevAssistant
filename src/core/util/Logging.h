#ifndef DEVASS_LOGGING_H
#define DEVASS_LOGGING_H

// Structured logging with named categories using qCDebug/qCWarning/qCritical.
//
// Usage:
//   #include "utils/Logging.h"
//   qCDebug(logCmd) << "Executing" << cmd->name();
//   qCWarning(logSeq) << "Sequence failed" << seqName;
//
// Qt rules file (.pro):
//   CONFIG += c++17
//   # Logging categories are auto-generated from this header at compile time.
//   # Control output via QT_LOGGING_RULES environment variable:
//   #   set QT_LOGGING_RULES=*.debug=true          (all debug)
//   #   set QT_LOGGING_RULES=devass.cmd.debug=true  (only command debug)
//   #   set QT_LOGGING_RULES=devass.*.debug=false   (suppress all devass debug)

#include <QLoggingCategory>

// Core
Q_DECLARE_LOGGING_CATEGORY(logCore)        // DevAssManager, ContextManager, ModelAdapter
Q_DECLARE_LOGGING_CATEGORY(logCmd)         // CommandParser, CommandRegistry, all Commands
Q_DECLARE_LOGGING_CATEGORY(logPrompt)      // PromptResolver

// UI / Controllers
Q_DECLARE_LOGGING_CATEGORY(logUI)          // MainWindow, dialogs
Q_DECLARE_LOGGING_CATEGORY(logSeq)         // SequenceRunner, SequenceEditorController
Q_DECLARE_LOGGING_CATEGORY(logProject)     // ProjectBrowserController

// Code viewer / graph
Q_DECLARE_LOGGING_CATEGORY(logViewer)      // SequenceRelationView, SequenceGraphWidget
Q_DECLARE_LOGGING_CATEGORY(logTemplate)    // SolutionTemplateManager, SolutionTemplateEditor, TemplateSequenceGraphWidget

// Audio / external
Q_DECLARE_LOGGING_CATEGORY(logAudio)       // FasterWhisperRunner, OmniAsrRunner
Q_DECLARE_LOGGING_CATEGORY(logRag)         // LightRagWidget

// Utils
Q_DECLARE_LOGGING_CATEGORY(logUtils)       // DescriptionGenerator, misc utilities

#endif // DEVASS_LOGGING_H
