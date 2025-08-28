QT += core gui widgets

CONFIG += c++17 object_parallel_to_source
TEMPLATE = app
TARGET = TonyIDE

DEFINES += __USE_MINGW_ANSI_STDIO=1
QMAKE_CFLAGS   += -D__USE_MINGW_ANSI_STDIO=1 -std=c11
QMAKE_CXXFLAGS += -D__USE_MINGW_ANSI_STDIO=1

# ВАЖНО: наши инклюды идут первыми, чтобы не схватывать чужие <unicode/utf8.h>
INCLUDEPATH = \
    $$PWD/tree-sitter/lib/src \
    $$PWD/tree-sitter/lib/include \
    $$INCLUDEPATH

SOURCES += \
    astgraphview.cpp \
    codeeditor.cpp \
    cpphighlighter.cpp \
    main.cpp \
    mainwindow.cpp \
    methodeditdialog.cpp \
    $$PWD/tree-sitter/lib/src/lib.c \
    $$PWD/tree-sitter-cpp/src/parser.c \
    $$PWD/tree-sitter-cpp/src/scanner.c \
    #$$PWD/tree-sitter-c/src/parser.c \
    #$$PWD/tree-sitter-c/src/scanner.c \
    #$$PWD/tree-sitter-python/src/parser.c \
    #$$PWD/tree-sitter-python/src/scanner.c \
    #$$PWD/tree-sitter-dart/src/parser.c \
    #$$PWD/tree-sitter-dart/src/scanner.c

HEADERS += \
    astgraphview.h \
    codeeditor.h \
    cpphighlighter.h \
    mainwindow.h \
    methodeditdialog.h
