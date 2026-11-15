# Compiler settings shared by the core library, the app and the tests.
CONFIG += c++17 warn_on object_parallel_to_source
# One configuration per build folder: CONFIG+=release or CONFIG+=debug on the qmake line.
CONFIG -= debug_and_release debug_and_release_target

QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
QMAKE_CFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
# All C sources are third-party (tree-sitter, md4c): keep their warnings out of the build log.
QMAKE_CFLAGS += -Wno-format -Wno-format-extra-args -Wno-unused-parameter -Wno-unused-value -Wno-unused-function

# Includes are written from src/, for example #include "core/commands/Command.h".
INCLUDEPATH += \
    $$DEVASS_SRC \
    $$DEVASS_THIRD_PARTY/tree-sitter/lib/include
