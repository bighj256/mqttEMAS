#include "widget.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置应用程序信息

    QApplication::setApplicationName("MQTT环境监测系统");

    Widget w;
    w.show();
    return a.exec();
}
