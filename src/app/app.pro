# The desktop program: the user interface (src/ui), voice (src/audio) and main().
TEMPLATE = app
TARGET = DevAssistantQt
QT += widgets multimedia

include(../devassistant.pri)
include(../core/core.pri)
DESTDIR = $$DEVASS_BIN_DIR

UI_SRC = $$DEVASS_SRC/ui
AUDIO_SRC = $$DEVASS_SRC/audio

HEADERS += \
    $$files($$UI_SRC/*.h, true) \
    $$files($$AUDIO_SRC/*.h, true)
SOURCES += \
    $$PWD/main.cpp \
    $$files($$UI_SRC/*.cpp, true) \
    $$files($$AUDIO_SRC/*.cpp, true)
FORMS += $$UI_SRC/editor/IDEWidget.ui
RESOURCES += $$PWD/resources.qrc

# md4c renders the model's Markdown answers.
INCLUDEPATH += $$DEVASS_THIRD_PARTY/md4c/src
SOURCES += \
    $$DEVASS_THIRD_PARTY/md4c/src/entity.c \
    $$DEVASS_THIRD_PARTY/md4c/src/md4c.c \
    $$DEVASS_THIRD_PARTY/md4c/src/md4c-html.c

# Russian translation. The build generates translations/devassistant_ru.ts with lupdate and
# fills it from translations/ru/*.json (scripts/fill_translations.py); qmake then compiles
# it into the program, available as :/i18n/devassistant_ru.qm.
exists($$DEVASS_ROOT/translations/devassistant_ru.ts) {
    TRANSLATIONS += $$DEVASS_ROOT/translations/devassistant_ru.ts
    CONFIG += lrelease embed_translations
}
