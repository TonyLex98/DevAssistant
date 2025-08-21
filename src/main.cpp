#include <QApplication>
#include "ui/MainWindow.h"
#include <VoskWidget.h>

/*
int main(int argc, char** argv){
    QApplication app(argc, argv);
    MainWindow w; w.resize(1000,600); w.show();
    return app.exec();
}*/



int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    VoskWidget w;
    w.show();

    return app.exec();
}
