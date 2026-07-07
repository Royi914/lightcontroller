#include <QApplication>
#include <QDate>
#include <QMessageBox>
#include "mainwindow.h"

static const QDate EXPIRY(2026, 8, 15);

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LightController");

    if (QDate::currentDate() > EXPIRY) {
        QMessageBox::critical(nullptr, "试用期已过", "试用期已过，试用版已过期");
        return 1;
    }

    MainWindow w;
    w.setWindowTitle(w.windowTitle() + " - 试用版");
    w.show();

    return app.exec();
}
