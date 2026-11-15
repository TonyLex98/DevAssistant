# The command core: parsing, policy, every command, DevAssManager and the change applier.
# CommandConfirmDialog (QtWidgets) is left out; nothing here needs it.

HEADERS += \
    $$SRC/core/DevAssManager.h

SOURCES += \
    $$SRC/core/DevAssManager.cpp \
    $$SRC/core/ChangeApplier.cpp \
    $$SRC/utils/Logging.cpp \
    $$SRC/utils/PathGuard.cpp \
    $$SRC/utils/TextFileLines.cpp \
    $$SRC/utils/json_utils.cpp \
    $$SRC/command/CommandParser.cpp \
    $$SRC/command/CommandRegistry.cpp \
    $$SRC/command/CommandDocs.cpp \
    $$SRC/command/CommandPolicy.cpp

# Command.cpp and every <Name>Command.cpp.
SOURCES += $$files($$SRC/command/*Command.cpp)
