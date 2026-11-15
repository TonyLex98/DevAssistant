
TEMPLATE = app
TARGET = DevAssistantQt
CONFIG += qt warn_on release
QT += widgets network multimedia core gui sql
QMAKE_CXXFLAGS += -std=gnu++1z
QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
QMAKE_CFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8

CONFIG += c++17 object_parallel_to_source


INCLUDEPATH += \
    src src/core \
    ragmodule \
    src/adapters \
    src/ui \
    src/utils \
    src/command  \
    src/audio \
    src/uiide \
    src/ui/customWidgets \
    src/codeviewer

INCLUDEPATH += \
    $$PWD/src/uiide/tree-sitter/lib/src \
    $$PWD/src/uiide/tree-sitter/lib/include


SOURCES += \
    src/audio/MicTranscriber.cpp \
    src/audio/VoskDyn.cpp \
    src/audio/VoskRunner.cpp \
    src/audio/WavWriter.cpp \
    src/audio/OmniAsrRunner.cpp \
    src/audio/FasterWhisperRunner.cpp \
    src/audio/TtsRunner.cpp \
    src/audio/PythonScripts.cpp \
    src/audio/SpeechRecognizer.cpp \
    src/core/ContextManager.cpp \
    src/core/ContextChainResolver.cpp \
    src/codeviewer/CodePatcher.cpp \
    src/codeviewer/GraphItems.cpp \
    src/codeviewer/GraphLayout.cpp \
    src/codeviewer/GraphWidget.cpp \
    src/codeviewer/ProjectIndexer.cpp \
    src/codeviewer/ProjectRelationView.cpp \
    src/codeviewer/SequenceGraphWidget.cpp \
    src/codeviewer/SequenceRelationView.cpp \
    src/codeviewer/TemplateSequenceGraphWidget.cpp \
    src/command/Command.cpp \
    src/command/CommandConfirmDialog.cpp \
    src/command/CommandPolicy.cpp \
    src/command/CommandParser.cpp \
    src/command/CommandRegistry.cpp \
    src/command/CommandDocs.cpp \
    src/command/CreateFileCommand.cpp \
    src/command/DeleteFileCommand.cpp \
    src/command/EditInRangeCommand.cpp \
    src/command/OpenFileContextCommand.cpp \
    src/command/RenameFileCommand.cpp \
    src/command/ReplaceInFileCommand.cpp \
    src/command/SearchProjectCommand.cpp \
    src/command/SearchDocsCommand.cpp \
    src/command/SearchProjectCaseInsensitiveCommand.cpp \
    src/command/SearchWebCommand.cpp \
    src/command/ShowResultCommand.cpp \
    src/command/CallSequenceCommand.cpp \
    src/command/RunPythonCommand.cpp \
    src/command/RunShellCommand.cpp \
    src/command/WriteDescriptionCommand.cpp \
    src/command/ListFilesInFolderCommand.cpp \
    src/command/ReasoningCommand.cpp \
    src/command/CopyFileCommand.cpp \
    src/command/CreateDirCommand.cpp \
    src/command/DeleteDirCommand.cpp \
    src/command/AppendToFileCommand.cpp \
    src/command/InsertAtLineCommand.cpp \
    src/command/ApplyPatchCommand.cpp \
    src/command/GitStatusCommand.cpp \
    src/command/GitDiffCommand.cpp \
    src/command/GitRestoreFileCommand.cpp \
    src/command/UndoLastChangeCommand.cpp \
    src/command/ContextShowCommand.cpp \
    src/command/ContextClearCommand.cpp \
    src/core/DevAssManager.cpp \
    src/core/PromptResolver.cpp \
    src/core/PromptSequence.cpp \
    src/core/PromptSequenceManager.cpp \
    src/core/PromptStep.cpp \
    src/main.cpp \
    src/core/ProjectIndex.cpp \
    src/core/ProjectStructureResolver.cpp \
    src/core/ChangeApplier.cpp \
    src/adapters/OllamaAdapter.cpp \
    src/ui/InputTextEdit.cpp \
    src/ui/MainWindow.cpp \
    src/ui/ChatDialog.cpp \
    src/ui/ContextGraphWidget.cpp \
    src/ui/ResultDisplayWidget.cpp \
    src/ui/OllamaSettingsDialog.cpp \
    src/ui/PromptEditorDialog.cpp \
    src/ui/ChangesDialog.cpp \
    src/ui/LightRagClient.cpp \
    src/ui/LightRagWidget.cpp \
    src/ui/PromptSequenceEditor.cpp \
    src/ui/PromptStepDialog.cpp \
    src/ui/SolutionTemplateEditor.cpp \
    src/ui/customWidgets/MemoryComboBox.cpp \
    src/ui/customWidgets/MemoryLineEdit.cpp \
    src/ui/customWidgets/MemorySplitter.cpp \
    src/ui/customWidgets/StyleTextEdit.cpp \
    src/ui/customWidgets/StyledButton.cpp \
    src/ui/customWidgets/StyledCheckBox.cpp \
    src/ui/customWidgets/StyledLabel.cpp \
    src/ui/customWidgets/StyledListWidget.cpp \
    src/ui/customWidgets/StyledSpinBox.cpp \
    src/ui/customWidgets/StyledTabWidget.cpp \
    src/ui/customWidgets/StyledTextEdit.cpp \
    src/ui/customWidgets/StyledTreeWidget.cpp \
    src/ui/customWidgets/CustomTextTabBar.cpp \
    src/uiide/DartHighlighter.cpp \
    src/uiide/IDEWidget.cpp \
    src/uiide/PathColorDelegate.cpp \
    src/uiide/PythonHighlighter.cpp \
    src/uiide/astgraphview.cpp \
    src/uiide/codeeditor.cpp \
    src/uiide/cpphighlighter.cpp \
    src/uiide/methodeditdialog.cpp \
    src/utils/DescriptionGenerator.cpp \
    src/utils/PromptManager.cpp\
    src/utils/json_utils.cpp \
    src/utils/PathGuard.cpp \
    src/utils/TextFileLines.cpp \
    ragmodule/RagStore.cpp \
    ragmodule/RagText.cpp \
    ragmodule/RagEmbedder.cpp \
    ragmodule/RagIndex.cpp \
    ragmodule/RagWidget.cpp \
    src/uiide/tree-sitter/lib/src/lib.c \
    src/uiide/tree-sitter-cpp/src/parser.c \
    src/uiide/tree-sitter-cpp/src/scanner.c \
    src/uiide/tree-sitter-python/src/parser.c \
    src/uiide/tree-sitter-python/src/scanner.c \
    src/uiide/tree-sitter-dart/src/parser.c \
    src/uiide/tree-sitter-dart/src/scanner.c \
    src/md4c/src/entity.c \
    src/md4c/src/md4c.c \
    src/md4c/src/md4c-html.c

HEADERS += \
    src/audio/MicTranscriber.h \
    src/audio/VoskDyn.h \
    src/audio/VoskRunner.h \
    src/audio/WavWriter.h \
    src/audio/OmniAsrRunner.h \
    src/audio/FasterWhisperRunner.h \
    src/audio/TtsRunner.h \
    src/audio/PythonScripts.h \
    src/audio/SpeechRecognizer.h \
    src/core/ContextManager.h \
    src/core/ContextChainResolver.h \
    src/codeviewer/CodePatcher.h \
    src/codeviewer/GraphItems.h \
    src/codeviewer/GraphLayout.h \
    src/codeviewer/GraphWidget.h \
    src/codeviewer/ProjectIndexer.h \
    src/codeviewer/ProjectRelationView.h \
    src/codeviewer/SequenceGraphWidget.h \
    src/codeviewer/SequenceRelationView.h \
    src/codeviewer/TemplateSequenceGraphWidget.h \
    src/codeviewer/SymbolModel.h \
    src/command/Command.h \
    src/command/CommandConfirmDialog.h \
    src/command/CommandPolicy.h \
    src/utils/PathGuard.h \
    src/utils/TextFileLines.h \
    ragmodule/RagTypes.h \
    ragmodule/RagStore.h \
    ragmodule/RagText.h \
    ragmodule/RagEmbedder.h \
    ragmodule/RagIndex.h \
    ragmodule/RagWidget.h \
    src/command/CommandParser.h \
    src/command/CommandRegistry.h \
    src/command/CommandDocs.h \
    src/command/CreateFileCommand.h \
    src/command/DeleteFileCommand.h \
    src/command/EditInRangeCommand.h \
    src/command/OpenFileContextCommand.h \
    src/command/RenameFileCommand.h \
    src/command/ReplaceInFileCommand.h \
    src/command/SearchProjectCommand.h \
    src/command/SearchDocsCommand.h \
    src/command/SearchProjectCaseInsensitiveCommand.h \
    src/command/SearchWebCommand.h \
    src/command/ShowResultCommand.h \
    src/command/CallSequenceCommand.h \
    src/command/RunPythonCommand.h \
    src/command/RunShellCommand.h \
    src/command/WriteDescriptionCommand.h \
    src/command/ListFilesInFolderCommand.h \
    src/command/ReasoningCommand.h \
    src/command/CopyFileCommand.h \
    src/command/CreateDirCommand.h \
    src/command/DeleteDirCommand.h \
    src/command/AppendToFileCommand.h \
    src/command/InsertAtLineCommand.h \
    src/command/ApplyPatchCommand.h \
    src/command/GitStatusCommand.h \
    src/command/GitDiffCommand.h \
    src/command/GitRestoreFileCommand.h \
    src/command/UndoLastChangeCommand.h \
    src/command/ContextShowCommand.h \
    src/command/ContextClearCommand.h \
    src/core/DevAssManager.h \
    src/core/ProjectIndex.h \
    src/core/ProjectStructureResolver.h \
    src/core/ChangeApplier.h \
    src/core/ModelAdapter.h \
    src/adapters/OllamaAdapter.h \
    src/core/PromptResolver.h \
    src/core/PromptSequence.h \
    src/core/PromptSequenceManager.h \
    src/core/PromptStep.h \
    src/core/SolutionTemplate.h \
    src/core/SolutionTemplateManager.h \
    src/ui/InputTextEdit.h \
    src/ui/MainWindow.h \
    src/ui/ChatDialog.h \
    src/ui/ContextGraphWidget.h \
    src/ui/ResultDisplayWidget.h \
    src/ui/OllamaSettingsDialog.h \
    src/ui/PromptEditorDialog.h \
    src/ui/ChangesDialog.h \
    src/ui/LightRagClient.h \
    src/ui/LightRagWidget.h \
    src/ui/PromptSequenceEditor.h \
    src/ui/PromptStepDialog.h \
    src/ui/SolutionTemplateEditor.h \
    src/ui/RunTemplateOrSequenceDialog.h \
    src/ui/SolutionTemplateViewer.h \
    src/ui/customWidgets/MemoryComboBox.h \
    src/ui/customWidgets/MemoryLineEdit.h \
    src/ui/customWidgets/MemorySplitter.h \
    src/ui/customWidgets/StyleTextEdit.h \
    src/ui/customWidgets/StyledButton.h \
    src/ui/customWidgets/StyledCheckBox.h \
    src/ui/customWidgets/StyledLabel.h \
    src/ui/customWidgets/StyledListWidget.h \
    src/ui/customWidgets/StyledSpinBox.h \
    src/ui/customWidgets/StyledTabWidget.h \
    src/ui/customWidgets/StyledTextEdit.h \
    src/ui/customWidgets/StyledTreeWidget.h \
    src/ui/customWidgets/CustomTextTabBar.h \
    src/uiide/DartHighlighter.h \
    src/uiide/IDEWidget.h \
    src/uiide/PathColorDelegate.h \
    src/uiide/PythonHighlighter.h \
    src/uiide/astgraphview.h \
    src/uiide/codeeditor.h \
    src/uiide/cpphighlighter.h \
    src/uiide/methodeditdialog.h \
    src/utils/DescriptionGenerator.h \
    src/utils/PromptManager.h


#INCLUDEPATH += $$PWD/external/whisper.cpp
#LIBS += -L$$PWD/external/whisper.cpp/bin -lwhisper


SOURCES += $$PWD/src/utils/AppConfig.cpp
SOURCES += $$PWD/src/command/OpenFileRegionCommand.cpp
SOURCES += $$PWD/src/command/AskUserCommand.cpp
SOURCES += $$PWD/src/command/OpenAppCommand.cpp
SOURCES += $$PWD/src/command/OpenUrlCommand.cpp
SOURCES += $$PWD/src/command/ClipboardSetCommand.cpp
SOURCES += $$PWD/src/command/ClipboardGetCommand.cpp
SOURCES += $$PWD/src/core/SolutionTemplate.cpp
SOURCES += $$PWD/src/core/SolutionTemplateManager.cpp
SOURCES += $$PWD/src/ui/RunTemplateOrSequenceDialog.cpp
SOURCES += $$PWD/src/ui/SolutionTemplateViewer.cpp
SOURCES += $$PWD/src/ui/controllers/TtsController.cpp
SOURCES += $$PWD/src/ui/controllers/TaskStatusController.cpp
SOURCES += $$PWD/src/ui/controllers/ContextCacheManager.cpp
SOURCES += $$PWD/src/ui/controllers/SequenceRunner.cpp
SOURCES += $$PWD/src/ui/controllers/SequenceEditorController.cpp
SOURCES += $$PWD/src/ui/controllers/UiBuilder.cpp
HEADERS += $$PWD/src/utils/AppConfig.h
HEADERS += $$PWD/src/utils/json_utils.h
HEADERS += $$PWD/src/ui/controllers/SequenceRunner.h
HEADERS += $$PWD/src/ui/controllers/SequenceEditorController.h
HEADERS += $$PWD/src/ui/controllers/UiBuilder.h
HEADERS += $$PWD/src/command/OpenFileRegionCommand.h
HEADERS += $$PWD/src/command/AskUserCommand.h
HEADERS += $$PWD/src/command/OpenAppCommand.h
HEADERS += $$PWD/src/command/OpenUrlCommand.h
HEADERS += $$PWD/src/command/ClipboardSetCommand.h
HEADERS += $$PWD/src/command/ClipboardGetCommand.h
HEADERS += $$PWD/src/ui/controllers/TtsController.h
HEADERS += $$PWD/src/ui/controllers/TaskStatusController.h
HEADERS += $$PWD/src/ui/controllers/ContextCacheManager.h

FORMS += \
    src/uiide/IDEWidget.ui


HEADERS += $$PWD/src/utils/Logging.h
SOURCES += $$PWD/src/utils/Logging.cpp

RESOURCES += src/resources/resources.qrc
