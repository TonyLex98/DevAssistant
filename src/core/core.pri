# Links a project (the app, a test) against the core library.
QT += core gui network sql
LIBS += -L$$DEVASS_LIB_DIR -ldevass_core
PRE_TARGETDEPS += $$DEVASS_LIB_DIR/libdevass_core.a
