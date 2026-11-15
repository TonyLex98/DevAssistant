include(../common.pri)

TARGET = tst_commandpolicy

SOURCES += \
    tst_commandpolicy.cpp \
    $$SRC/command/CommandPolicy.cpp \
    $$SRC/utils/PathGuard.cpp
