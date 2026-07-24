#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/icon_vcr.svg"));
    gui::MainWindow window;
    window.show();
    return app.exec();
}
