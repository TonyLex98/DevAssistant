
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
    src/codeviwer/CodePatcher.cpp \
    src/codeviwer/GraphItems.cpp \
    src/codeviwer/GraphLayout.cpp \
    src/codeviwer/ProjectIndexer.cpp \
    src/codeviwer/ProjectRelationView.cpp \
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
    src/command/WriteDescriptionCommand.cpp \
    src/core/DevAssManager.cpp \
    src/core/PromptResolver.cpp \
    src/core/PromptSequence.cpp \
    src/core/PromptSequenceManager.cpp \
    src/core/PromptStep.cpp \
    src/main.cpp \
    src/core/ProjectIndex.cpp \
    src/core/ChangeApplier.cpp \
    src/adapters/OllamaAdapter.cpp \
    src/ui/DescriptionDialog.cpp \
    src/ui/InputTextEdit.cpp \
    src/ui/MainWindow.cpp \
    src/ui/ChatDialog.cpp \
    src/ui/ModelsDialog.cpp \
    src/ui/OllamaSettingsDialog.cpp \
    src/ui/PromptEditorDialog.cpp \
    src/ui/ChangesDialog.cpp \
    src/ui/PromptSequenceEditor.cpp \
    src/ui/PromptStepDialog.cpp \
    src/ui/RunSequenceDialog.cpp \
    src/ui/customWidgets/MemoryComboBox.cpp \
    src/ui/customWidgets/MemoryLineEdit.cpp \
    src/ui/customWidgets/MemorySplitter.cpp \
    src/ui/customWidgets/StyleTextEdit.cpp \
    src/ui/customWidgets/StyledButton.cpp \
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
    src/uiide/tree-sitter-dart/src/scanner.c

HEADERS += \
    src/audio/MicStreamWidget.h \
    src/audio/MicTranscriber.h \
    src/audio/StreamRunner.h \
    src/audio/VoskDyn.h \
    src/audio/VoskRunner.h \
    src/audio/VoskWidget.h \
    src/audio/WavWriter.h \
    src/codeviwer/CodePatcher.h \
    src/codeviwer/GraphItems.h \
    src/codeviwer/GraphLayout.h \
    src/codeviwer/ProjectIndexer.h \
    src/codeviwer/ProjectRelationView.h \
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
    src/command/WriteDescriptionCommand.h \
    src/core/DevAssManager.h \
    src/core/ProjectIndex.h \
    src/core/ChangeApplier.h \
    src/core/ModelAdapter.h \
    src/adapters/OllamaAdapter.h \
    src/core/PromptResolver.h \
    src/core/PromptSequence.h \
    src/core/PromptSequenceManager.h \
    src/core/PromptStep.h \
    src/ui/DescriptionDialog.h \
    src/ui/InputTextEdit.h \
    src/ui/MainWindow.h \
    src/ui/ChatDialog.h \
    src/ui/ModelsDialog.h \
    src/ui/OllamaSettingsDialog.h \
    src/ui/PromptEditorDialog.h \
    src/ui/ChangesDialog.h \
    src/ui/PromptSequenceEditor.h \
    src/ui/PromptStepDialog.h \
    src/ui/RunSequenceDialog.h \
    src/ui/customWidgets/MemoryComboBox.h \
    src/ui/customWidgets/MemoryLineEdit.h \
    src/ui/customWidgets/MemorySplitter.h \
    src/ui/customWidgets/StyleTextEdit.h \
    src/ui/customWidgets/StyledButton.h \
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
HEADERS += $$PWD/src/utils/AppConfig.h
HEADERS += $$PWD/src/command/OpenFileRegionCommand.h
HEADERS += $$PWD/src/command/AskUserCommand.h

FORMS += \
    src/uiide/IDEWidget.ui

