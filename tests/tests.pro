# Unit tests for DevAssistant. Build out of source, for example:
#   mkdir build-tests && cd build-tests
#   qmake ../tests/tests.pro CONFIG+=release && mingw32-make && mingw32-make check
# The code viewer test needs the tree-sitter sources (scripts/fetch_tree_sitter.ps1).

TEMPLATE = subdirs

SUBDIRS += \
    tst_pathguard \
    tst_textfilelines \
    tst_jsonutils \
    tst_changeapplier \
    tst_commandpolicy \
    tst_commandparser \
    tst_filecommands \
    tst_promptdata \
    tst_codeviewer
