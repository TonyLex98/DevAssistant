# Settings shared by every test target.

QT += core gui network testlib
QT -= widgets

CONFIG += c++17 console testcase warn_on object_parallel_to_source
CONFIG -= app_bundle debug_and_release

QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
QMAKE_CFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8

SRC = $$clean_path($$PWD/../src)

INCLUDEPATH += \
    $$PWD \
    $$SRC \
    $$SRC/core \
    $$SRC/command \
    $$SRC/utils \
    $$SRC/codeviewer

HEADERS += $$PWD/TestFiles.h
