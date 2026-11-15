# The core library: everything the assistant does, without a user interface.
#
#   model/     clients for the model servers
#   project/   the project folder: files, changes with undo, path checks, descriptions
#   code/      the code index (tree-sitter) and code patching
#   commands/  the commands the model can use, their parser, registry and policy
#   context/   what the model is shown: executed commands, opened files, prompt templates
#   workflow/  prompt sequences, solution templates and the agent that runs them
#   web/       web search: fetching, the search providers, extractive summaries
#   rag/       document search and the LightRAG client
#   util/      settings, logging, JSON helpers
#
# QtWidgets is deliberately not linked: an include of a widget header here fails the build,
# which keeps the logic testable and the user interface replaceable.
TEMPLATE = lib
TARGET = devass_core
CONFIG += staticlib
QT = core gui network sql

include(../devassistant.pri)
DESTDIR = $$DEVASS_LIB_DIR

HEADERS += $$files($$PWD/*.h, true)
SOURCES += $$files($$PWD/*.cpp, true)

# tree-sitter and the grammars of the code index, downloaded by scripts/fetch_tree_sitter.ps1.
TS = $$DEVASS_THIRD_PARTY
!exists($$TS/tree-sitter/lib/src/lib.c): \
    error("tree-sitter sources are missing; run scripts/fetch_tree_sitter.ps1 first")
INCLUDEPATH += $$TS/tree-sitter/lib/src
SOURCES += \
    $$TS/tree-sitter/lib/src/lib.c \
    $$TS/tree-sitter-cpp/src/parser.c \
    $$TS/tree-sitter-cpp/src/scanner.c \
    $$TS/tree-sitter-python/src/parser.c \
    $$TS/tree-sitter-python/src/scanner.c \
    $$TS/tree-sitter-dart/src/parser.c \
    $$TS/tree-sitter-dart/src/scanner.c
