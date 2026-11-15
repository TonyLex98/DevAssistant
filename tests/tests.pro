# Unit tests for DevAssistant (QtTest). They are built with the rest of the project
# (DevAssistant.pro) and run with "mingw32-make check".
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
    tst_codeviewer \
    tst_session \
    tst_llmclient \
    tst_agentengine
