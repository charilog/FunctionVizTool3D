#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>
#include <QPoint>
#include "ObjectiveFunction.h"
#include <vector>

class SurfaceWidget final : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
public:
    explicit SurfaceWidget(QWidget* parent=nullptr);
    ~SurfaceWidget() override;

    void setObjective(const ObjectiveFunction& obj);
    void setDimension(int d);
    void setAxes(int xAxis, int yAxis);
    void setGridN(int n);
    void setBounds(const std::vector<double>& lower, const std::vector<double>& upper);
    void setFixed(const std::vector<double>& fixed);
    void setWireframe(bool w);
    void setZScale(double s);

    void rebuildSurface();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    struct Vertex {
        float px, py, pz;
        float nx, ny, nz;
        float r, g, b;
    };

    void clearGL();
    bool ensureProgram();
    void buildMeshCPU();
    void uploadMeshGL();

    QMatrix4x4 projection() const;
    QMatrix4x4 view() const;

    void drawAxes(const QMatrix4x4& mvp);

private:
    ObjectiveFunction obj_;
    int dim_{2};
    int xAxis_{0};
    int yAxis_{1};
    int gridN_{81};
    bool wireframe_{false};
    double zScale_{1.0};

    std::vector<double> lower_, upper_, fixed_;

    std::vector<Vertex> vertices_;
    std::vector<unsigned int> indices_;

    float zMin_{0.f}, zMax_{1.f};

    // GL objects
    QOpenGLShaderProgram* prog_{nullptr};
    unsigned int vao_{0}, vbo_{0}, ebo_{0};

    // Camera
    QPoint lastPos_;
    float yaw_{-35.f};
    float pitch_{35.f};
    float distance_{3.2f};
    QVector3D pan_{0.f, 0.f, 0.f};
};
