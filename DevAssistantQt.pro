
TEMPLATE = app
TARGET = DevAssistantQt
CONFIG += qt warn_on release
QT += widgets network multimedia
QMAKE_CXXFLAGS += -std=gnu++1z

INCLUDEPATH += src src/core src/adapters src/ui src/utils src/command  src/audio

SOURCES += \
    src/audio/MicStreamWidget.cpp \
    src/audio/MicTranscriber.cpp \
    src/audio/StreamRunner.cpp \
    src/audio/VoskDyn.cpp \
    src/audio/VoskRunner.cpp \
    src/audio/VoskWidget.cpp \
    src/audio/WavWriter.cpp \
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
    src/main.cpp \
    src/core/ProjectIndex.cpp \
    src/core/ChangeApplier.cpp \
    src/adapters/OllamaAdapter.cpp \
    src/ui/DescriptionDialog.cpp \
    src/ui/MainWindow.cpp \
    src/ui/ChatDialog.cpp \
    src/ui/ModelsDialog.cpp \
    src/ui/OllamaSettingsDialog.cpp \
    src/ui/PromptEditorDialog.cpp \
    src/ui/ChangesDialog.cpp \
    src/utils/DescriptionGenerator.cpp \
    src/utils/PromptManager.cpp

HEADERS += \
    src/audio/MicStreamWidget.h \
    src/audio/MicTranscriber.h \
    src/audio/StreamRunner.h \
    src/audio/VoskDyn.h \
    src/audio/VoskRunner.h \
    src/audio/VoskWidget.h \
    src/audio/WavWriter.h \
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
    src/ui/DescriptionDialog.h \
    src/ui/MainWindow.h \
    src/ui/ChatDialog.h \
    src/ui/ModelsDialog.h \
    src/ui/OllamaSettingsDialog.h \
    src/ui/PromptEditorDialog.h \
    src/ui/ChangesDialog.h \
    src/utils/DescriptionGenerator.h \
    src/utils/PromptManager.h


#INCLUDEPATH += $$PWD/external/whisper.cpp
#LIBS += -L$$PWD/external/whisper.cpp/bin -lwhisper

