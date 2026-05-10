#include "GameWidget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    GameWidget widget;
    widget.setWindowTitle(QStringLiteral("James Runner"));
    widget.resize(1280, 720);
    widget.show();

    return app.exec();
}
