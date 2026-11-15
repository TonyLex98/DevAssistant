include(../common.pri)

TARGET = tst_codeviewer

TS = $$SRC/uiide
!exists($$TS/tree-sitter/lib/src/lib.c): \
    error("tree-sitter sources are missing; run scripts/fetch_tree_sitter.ps1 first")

INCLUDEPATH += \
    $$TS/tree-sitter/lib/include \
    $$TS/tree-sitter/lib/src

HEADERS += \
    $$SRC/codeviewer/ProjectIndexer.h \
    $$SRC/codeviewer/CodePatcher.h \
    $$SRC/codeviewer/SymbolModel.h

SOURCES += \
    tst_codeviewer.cpp \
    $$SRC/codeviewer/ProjectIndexer.cpp \
    $$SRC/codeviewer/CodePatcher.cpp \
    $$TS/tree-sitter/lib/src/lib.c \
    $$TS/tree-sitter-cpp/src/parser.c \
    $$TS/tree-sitter-cpp/src/scanner.c \
    $$TS/tree-sitter-python/src/parser.c \
    $$TS/tree-sitter-python/src/scanner.c \
    $$TS/tree-sitter-dart/src/parser.c \
    $$TS/tree-sitter-dart/src/scanner.c

# The generated grammar tables are huge; optimizing them only slows the build down.
QMAKE_CFLAGS_RELEASE -= -O2
QMAKE_CFLAGS_RELEASE += -O0
