#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("The Network Project");
    app.setOrganizationName("TNP");

    MainWindow w;
    w.resize(1200, 800);
    w.show();

    return app.exec();
}
