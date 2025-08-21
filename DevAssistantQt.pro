
TEMPLATE = app
TARGET = DevAssistantQt
CONFIG += qt warn_on release
QT += widgets network
QMAKE_CXXFLAGS += -std=gnu++1z

INCLUDEPATH += src src/core src/adapters src/ui src/utils src/command

SOURCES += \
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
    src/utils/PromptManager.cpp

HEADERS += \
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
    src/utils/PromptManager.h
