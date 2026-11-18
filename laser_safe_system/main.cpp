#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    // 高 DPI 支持
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    QApplication app(argc, argv);
    app.setApplicationName("LaserSafeSystem");
    app.setApplicationVersion("1.0");
    MainWindow w;
    w.show();

    return app.exec();
}
