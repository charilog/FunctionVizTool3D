#include <QApplication>
#include <QCoreApplication>
#include <QSurfaceFormat>
#include "MainWindow.h"

int main(int argc, char** argv)
{
    // Prefer desktop OpenGL for QOpenGLWidget on Windows.
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setMajorVersion(3);
    fmt.setMinorVersion(3);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSamples(4);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName("FunctionVizTool3D");
    app.setApplicationDisplayName("FunctionVizTool 3D Surface (standalone)");
    app.setOrganizationName("Standalone");

    MainWindow w;
    w.show();

    return app.exec();
}
