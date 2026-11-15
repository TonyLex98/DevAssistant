# Settings shared by every test target: each test links the core library.
QT += core gui network testlib
QT -= widgets
CONFIG += console testcase
CONFIG -= app_bundle

include($$DEVASS_SRC/devassistant.pri)
include($$DEVASS_SRC/core/core.pri)

INCLUDEPATH += $$PWD
HEADERS += $$PWD/TestFiles.h
