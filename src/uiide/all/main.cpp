#include <QApplication>
#include <QFileDialog>
#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include "ProjectRelationView.h"
#include <QMessageBox>

int main(int argc, char *argv[]){
    QApplication app(argc, argv);
    QString root;
    if(argc>1) root = QString::fromLocal8Bit(argv[1]);
    if(root.isEmpty()) root = QFileDialog::getExistingDirectory(nullptr, "Выберите корень проекта");
    if(root.isEmpty()) return 0;

    auto *win = new QMainWindow;
    auto *tabs = new QTabWidget(win);
    auto *view = new ProjectRelationView;
    view->setProjectRoot(root);
    QObject::connect(view, &ProjectRelationView::openInEditor, [&](const QString &file, int line){
        // Demo: просто сообщим
        QMessageBox::information(win, "Open", QString("Открыть файл:\n%1:%2").arg(file).arg(line));
    });
    tabs->addTab(view, "Граф проекта");
    win->setCentralWidget(tabs);
    win->resize(1280, 800);
    win->show();
    return app.exec();
}
