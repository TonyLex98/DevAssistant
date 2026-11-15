# DevAssistant: the core library, the desktop program and the tests.
#
#   qmake DevAssistant.pro CONFIG+=release
#   mingw32-make -j4          # the program lands in bin/, the core library in lib/
#   mingw32-make check        # runs the tests
#
# A shadow build (qmake run from another folder) puts bin/ and lib/ there instead.
TEMPLATE = subdirs

SUBDIRS += core app tests
core.subdir = src/core
app.subdir = src/app
app.depends = core
tests.subdir = tests
tests.depends = core
