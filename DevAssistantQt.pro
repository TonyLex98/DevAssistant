
TEMPLATE = app
TARGET = DevAssistantQt
CONFIG += qt warn_on release
QT += widgets network
QMAKE_CXXFLAGS += -std=gnu++1z

INCLUDEPATH += src src/core src/adapters src/scenarios src/ui src/utils

SOURCES += \
    src/core/DevAssManager.cpp \
    src/main.cpp \
    src/core/ProjectIndex.cpp \
    src/core/ChangeApplier.cpp \
    src/adapters/OllamaAdapter.cpp \
    src/scenarios/CreateFileScenario.cpp \
    src/scenarios/ReplaceInFileScenario.cpp \
    src/scenarios/RenameFileScenario.cpp \
    src/scenarios/DeleteFileScenario.cpp \
    src/scenarios/EditInRangeScenario.cpp \
    src/scenarios/ScenarioRegistry.cpp \
    src/ui/DescriptionDialog.cpp \
    src/ui/MainWindow.cpp \
    src/ui/ChatDialog.cpp \
    src/ui/ModelsDialog.cpp \
    src/ui/OllamaSettingsDialog.cpp \
    src/ui/PromptEditorDialog.cpp \
    src/ui/ChangesDialog.cpp \
    src/utils/DslExtractor.cpp \
    src/utils/PromptManager.cpp

HEADERS += \
    src/core/DevAssManager.h \
    src/core/ProjectIndex.h \
    src/core/ChangeApplier.h \
    src/core/ModelAdapter.h \
    src/adapters/OllamaAdapter.h \
    src/scenarios/IScenario.h \
    src/scenarios/CreateFileScenario.h \
    src/scenarios/ReplaceInFileScenario.h \
    src/scenarios/RenameFileScenario.h \
    src/scenarios/DeleteFileScenario.h \
    src/scenarios/EditInRangeScenario.h \
    src/scenarios/ScenarioRegistry.h \
    src/ui/DescriptionDialog.h \
    src/ui/MainWindow.h \
    src/ui/ChatDialog.h \
    src/ui/ModelsDialog.h \
    src/ui/OllamaSettingsDialog.h \
    src/ui/PromptEditorDialog.h \
    src/ui/ChangesDialog.h \
    src/utils/DslExtractor.h \
    src/utils/PromptManager.h
