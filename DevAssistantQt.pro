
TEMPLATE = app
TARGET = DevAssistantQt
CONFIG += qt warn_on release
QT += widgets network multimedia core gui widgets
QMAKE_CXXFLAGS += -std=gnu++1z

CONFIG += c++17 object_parallel_to_source


INCLUDEPATH += \
    src src/core \
    src/adapters \
    src/ui \
    src/utils \
    src/command  \
    src/audio \
    src/uiide \
    src/ui/customWidgets \
    src/codeviwer

INCLUDEPATH += \
    $$PWD/src/uiide/tree-sitter/lib/src \
    $$PWD/src/uiide/tree-sitter/lib/include


SOURCES += \
    src/audio/MicStreamWidget.cpp \
    src/audio/MicTranscriber.cpp \
    src/audio/StreamRunner.cpp \
    src/audio/VoskDyn.cpp \
    src/audio/VoskRunner.cpp \
    src/audio/VoskWidget.cpp \
    src/audio/WavWriter.cpp \
    src/core/ContextManager.cpp \
    src/core/ContextChainResolver.cpp \
    src/codeviwer/CodePatcher.cpp \
    src/codeviwer/GraphItems.cpp \
    src/codeviwer/GraphLayout.cpp \
    src/codeviwer/GraphWidget.cpp \
    src/codeviwer/ProjectIndexer.cpp \
    src/codeviwer/ProjectRelationView.cpp \
    src/codeviwer/SequenceGraphWidget.cpp \
    src/codeviwer/SequenceRelationView.cpp \
    src/codeviwer/TemplateSequenceGraphWidget.cpp \
    src/command/Command.cpp \
    src/command/CommandConfirmDialog.cpp \
    src/command/CommandFactory.cpp \
    src/command/CommandParser.cpp \
    src/command/CommandRegistry.cpp \
    src/command/CreateFileCommand.cpp \
    src/command/DeleteFileCommand.cpp \
    src/command/EditInRangeCommand.cpp \
    src/command/OpenFileContextCommand.cpp \
    src/command/RenameFileCommand.cpp \
    src/command/ReplaceInFileCommand.cpp \
    src/command/SearchProjectCommand.cpp \
    src/command/SearchProjectCaseInsensitiveCommand.cpp \
    src/command/SearchWebCommand.cpp \
    src/command/ShowResultCommand.cpp \
    src/command/CallSequenceCommand.cpp \
    src/command/RunPythonCommand.cpp \
    src/command/RunShellCommand.cpp \
    src/command/WriteDescriptionCommand.cpp \
    src/command/ListFilesInFolderCommand.cpp \
    src/command/ReasoningCommand.cpp \
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
    src/ui/DescriptionDialog.cpp \
    src/ui/InputTextEdit.cpp \
    src/ui/MainWindow.cpp \
    src/ui/ChatDialog.cpp \
    src/ui/ResultDisplayWidget.cpp \
    src/ui/ModelsDialog.cpp \
    src/ui/OllamaSettingsDialog.cpp \
    src/ui/PromptEditorDialog.cpp \
    src/ui/ChangesDialog.cpp \
    src/ui/LightRagClient.cpp \
    src/ui/LightRagWidget.cpp \
    src/ui/OllamaClient.cpp \
    src/ui/PromptSequenceEditor.cpp \
    src/ui/PromptStepDialog.cpp \
    src/ui/RunSequenceDialog.cpp \
    src/ui/SolutionTemplateEditor.cpp \
    src/ui/customWidgets/MemoryComboBox.cpp \
    src/ui/customWidgets/MemoryLineEdit.cpp \
    src/ui/customWidgets/MemorySplitter.cpp \
    src/ui/customWidgets/StyleTextEdit.cpp \
    src/ui/customWidgets/StyledButton.cpp \
    src/ui/customWidgets/StyledCheckBox.cpp \
    src/ui/customWidgets/StyledLabel.cpp \
    src/ui/customWidgets/StyledListWidget.cpp \
    src/ui/customWidgets/StyledProgressBar.cpp \
    src/ui/customWidgets/StyledRadioButton.cpp \
    src/ui/customWidgets/StyledSpinBox.cpp \
    src/ui/customWidgets/StyledDoubleSpinBox.cpp \
    src/ui/customWidgets/StyledTabWidget.cpp \
    src/ui/customWidgets/StyledTextEdit.cpp \
    src/ui/customWidgets/StyledTreeWidget.cpp \
    src/ui/customWidgets/StyledPlainTextEdit.cpp \
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
    src/audio/MicStreamWidget.h \
    src/audio/MicTranscriber.h \
    src/audio/StreamRunner.h \
    src/audio/VoskDyn.h \
    src/audio/VoskRunner.h \
    src/audio/VoskWidget.h \
    src/audio/WavWriter.h \
    src/core/ContextManager.h \
    src/core/ContextChainResolver.h \
    src/codeviwer/CodePatcher.h \
    src/codeviwer/GraphItems.h \
    src/codeviwer/GraphLayout.h \
    src/codeviwer/GraphWidget.h \
    src/codeviwer/ProjectIndexer.h \
    src/codeviwer/ProjectRelationView.h \
    src/codeviwer/SequenceGraphWidget.h \
    src/codeviwer/SequenceRelationView.h \
    src/codeviwer/TemplateSequenceGraphWidget.h \
    src/codeviwer/SymbolModel.h \
    src/command/Command.h \
    src/command/CommandConfirmDialog.h \
    src/command/CommandFactory.h \
    src/command/CommandParser.h \
    src/command/CommandRegistry.h \
    src/command/CreateFileCommand.h \
    src/command/DeleteFileCommand.h \
    src/command/EditInRangeCommand.h \
    src/command/OpenFileContextCommand.h \
    src/command/RenameFileCommand.h \
    src/command/ReplaceInFileCommand.h \
    src/command/SearchProjectCommand.h \
    src/command/SearchProjectCaseInsensitiveCommand.h \
    src/command/SearchWebCommand.h \
    src/command/ShowResultCommand.h \
    src/command/CallSequenceCommand.h \
    src/command/RunPythonCommand.h \
    src/command/RunShellCommand.h \
    src/command/WriteDescriptionCommand.h \
    src/command/ListFilesInFolderCommand.h \
    src/command/ReasoningCommand.h \
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
    src/ui/DescriptionDialog.h \
    src/ui/InputTextEdit.h \
    src/ui/MainWindow.h \
    src/ui/ChatDialog.h \
    src/ui/ResultDisplayWidget.h \
    src/ui/ModelsDialog.h \
    src/ui/OllamaSettingsDialog.h \
    src/ui/PromptEditorDialog.h \
    src/ui/ChangesDialog.h \
    src/ui/LightRagClient.h \
    src/ui/LightRagWidget.h \
    src/ui/OllamaClient.h \
    src/ui/PromptSequenceEditor.h \
    src/ui/PromptStepDialog.h \
    src/ui/RunSequenceDialog.h \
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
    src/ui/customWidgets/StyledProgressBar.h \
    src/ui/customWidgets/StyledRadioButton.h \
    src/ui/customWidgets/StyledSpinBox.h \
    src/ui/customWidgets/StyledDoubleSpinBox.h \
    src/ui/customWidgets/StyledTabWidget.h \
    src/ui/customWidgets/StyledTextEdit.h \
    src/ui/customWidgets/StyledTreeWidget.h \
    src/ui/customWidgets/StyledPlainTextEdit.h \
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
SOURCES += $$PWD/src/core/SolutionTemplate.cpp
SOURCES += $$PWD/src/core/SolutionTemplateManager.cpp
SOURCES += $$PWD/src/ui/RunTemplateOrSequenceDialog.cpp
SOURCES += $$PWD/src/ui/SolutionTemplateViewer.cpp
HEADERS += $$PWD/src/utils/AppConfig.h
HEADERS += $$PWD/src/command/OpenFileRegionCommand.h
HEADERS += $$PWD/src/command/AskUserCommand.h

FORMS += \
    src/uiide/IDEWidget.ui

