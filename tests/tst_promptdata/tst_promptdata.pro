include(../common.pri)
include(../command_core.pri)

TARGET = tst_promptdata

SOURCES += \
    tst_promptdata.cpp \
    $$SRC/core/PromptStep.cpp \
    $$SRC/core/PromptSequence.cpp \
    $$SRC/core/SolutionTemplate.cpp
