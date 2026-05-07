#include "GameWidget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    GameWidget widget;
    widget.setWindowTitle(QStringLiteral("James Runner"));
    widget.resize(960, 540);
    widget.show();

    return app.exec();
}
