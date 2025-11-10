QT       += core gui network sql
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
SOURCES += \
    RAGProcessor.cpp \
    Database.cpp \
    Encoder.cpp \
    RagWidget.cpp
HEADERS += \
    RAGProcessor.h \
    Database.h \
    Encoder.h \
    RagWidget.h
# Enable SQLite support
# CONFIG += link_pkgconfig
# PKGCONFIG += sqlite3
# To use external tools (pdftotext, docx2txt, tesseract) the user must install them.
DISTFILES = \
    RAGProcessor.h \
    RAGProcessor.cpp \
    Database.h \
    Database.cpp \
    Encoder.h \
    Encoder.cpp \
    RagWidget.h \
    RagWidget.cpp
# Provide widget into parent application via header inclusion.
# The parent must add ``ragmodule/`` to include path and instantiate Rag::RagWidget.

