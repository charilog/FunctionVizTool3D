#include "SurfaceWidget.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMessageBox>
#include <algorithm>
#include <cmath>
#include <limits>

static float clampf(float v, float a, float b){ return (v<a)?a:(v>b)?b:v; }

SurfaceWidget::SurfaceWidget(QWidget* parent) : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    // A larger default camera distance avoids near-plane clipping for typical [-1,1]^2 surfaces.
    // Users can still zoom in/out with the mouse wheel.
    distance_ = 5.0f;
    lower_ = {-5.12, -5.12};
    upper_ = { 5.12,  5.12};
    fixed_ = {0.0, 0.0};
}

SurfaceWidget::~SurfaceWidget()
{
    makeCurrent();
    clearGL();
    doneCurrent();
}

void SurfaceWidget::setObjective(const ObjectiveFunction& obj){ obj_ = obj; }
void SurfaceWidget::setDimension(int d){ dim_ = d; lower_.assign(d, -5.0); upper_.assign(d, 5.0); fixed_.assign(d, 0.0); }
void SurfaceWidget::setAxes(int xAxis, int yAxis){ xAxis_ = xAxis; yAxis_ = yAxis; }
void SurfaceWidget::setGridN(int n){ gridN_ = n; }
void SurfaceWidget::setBounds(const std::vector<double>& lower, const std::vector<double>& upper){ lower_=lower; upper_=upper; }
void SurfaceWidget::setFixed(const std::vector<double>& fixed){ fixed_=fixed; }
void SurfaceWidget::setWireframe(bool w){ wireframe_=w; }
void SurfaceWidget::setZScale(double s){ zScale_=s; }

void SurfaceWidget::rebuildSurface()
{
    buildMeshCPU();

    // Auto-fit (only expands the distance). This prevents the surface from being clipped
    // when the user pans/rotates, especially when Z-scale is increased.
    {
        const float fovYdeg = 45.0f;
        const float halfFov = 0.5f * fovYdeg * (3.14159265358979323846f / 180.0f);
        const float z = 0.9f * float(std::max(0.0, zScale_));
        const float r = std::sqrt(1.0f*1.0f + 1.0f*1.0f + z*z);
        const float ideal = r / std::sin(halfFov) + 0.6f;
        if(distance_ < ideal) distance_ = ideal;
    }

    if(isValid()){
        makeCurrent();
        uploadMeshGL();
        doneCurrent();
    }
    update();
}

void SurfaceWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    ensureProgram();
    uploadMeshGL();
}

void SurfaceWidget::resizeGL(int w, int h)
{
    // QOpenGLWidget renders into an internal framebuffer whose size is in *physical* pixels.
    // On HiDPI displays, width()/height() are logical pixels. If glViewport() is fed logical
    // pixels, the scene renders into the lower-left portion and appears clipped while panning.
    const qreal dpr = devicePixelRatioF();
    const int fbw = int(std::lround(double(w) * double(dpr)));
    const int fbh = int(std::lround(double(h) * double(dpr)));
    glViewport(0, 0, std::max(1, fbw), std::max(1, fbh));
}

QMatrix4x4 SurfaceWidget::projection() const
{
    QMatrix4x4 p;
    const float aspect = float(width()) / float(std::max(1, height()));
    p.perspective(45.0f, aspect, 0.01f, 100.0f);
    return p;
}

QMatrix4x4 SurfaceWidget::view() const
{
    QMatrix4x4 v;
    v.translate(pan_);
    v.translate(0.f, 0.f, -distance_);
    v.rotate(pitch_, 1.f, 0.f, 0.f);
    v.rotate(yaw_, 0.f, 1.f, 0.f);
    return v;
}

void SurfaceWidget::paintGL()
{
    // Keep viewport in sync with the framebuffer size (HiDPI-safe).
    const qreal dpr = devicePixelRatioF();
    const int fbw = int(std::lround(double(width())  * double(dpr)));
    const int fbh = int(std::lround(double(height()) * double(dpr)));
    glViewport(0, 0, std::max(1, fbw), std::max(1, fbh));
    glClearColor(0.07f,0.07f,0.09f,1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if(!ensureProgram() || vao_==0 || indices_.empty()){
        return;
    }

    if(wireframe_) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    prog_->bind();

    const QMatrix4x4 mvp = projection() * view();
    prog_->setUniformValue("u_mvp", mvp);
    prog_->setUniformValue("u_lightDir", QVector3D(0.35f, 0.8f, 0.5f));

    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices_.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    // axes overlay in 3D (simple line draw using the same program and a tiny VAO-free path)
    drawAxes(mvp);

    prog_->release();

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void SurfaceWidget::drawAxes(const QMatrix4x4& mvp)
{
    // Quick & simple: draw 3 lines via immediate-mode-like upload to a small VBO-less draw.
    // Uses glDrawArrays with a temporary buffer.
    struct L { float x,y,z,r,g,b; };
    const float s = 1.15f;
    L lines[6] = {
        {-s,0,0,  0.9f,0.2f,0.2f}, { s,0,0,  0.9f,0.2f,0.2f}, // X
        {0,-s,0,  0.2f,0.9f,0.2f}, {0, s,0,  0.2f,0.9f,0.2f}, // Y
        {0,0,-s,  0.2f,0.4f,1.0f}, {0,0, s,  0.2f,0.4f,1.0f}, // Z
    };

    // Use the same shader, but interpret as position+color and set normal dummy.
    // We'll use a small VAO the first time.
    static unsigned int axVao=0, axVbo=0;
    if(axVao==0){
        glGenVertexArrays(1, &axVao);
        glGenBuffers(1, &axVbo);
        glBindVertexArray(axVao);
        glBindBuffer(GL_ARRAY_BUFFER, axVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(lines), lines, GL_DYNAMIC_DRAW);
        // layout matches Vertex: position (0), normal (1), color (2)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(L), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(L), (void*)0); // reuse pos as normal
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(L), (void*)(3*sizeof(float)));
        glBindVertexArray(0);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, axVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(lines), lines);
    }

    prog_->setUniformValue("u_mvp", mvp);
    glBindVertexArray(axVao);
    glDrawArrays(GL_LINES, 0, 6);
    glBindVertexArray(0);
}

void SurfaceWidget::mousePressEvent(QMouseEvent* e)
{
    lastPos_ = e->pos();
    e->accept();
}

void SurfaceWidget::mouseMoveEvent(QMouseEvent* e)
{
    const QPoint delta = e->pos() - lastPos_;
    lastPos_ = e->pos();

    if(e->buttons() & Qt::LeftButton){
        yaw_   += delta.x() * 0.35f;
        pitch_ += delta.y() * 0.35f;
        pitch_ = clampf(pitch_, -89.f, 89.f);
        update();
    } else if(e->buttons() & Qt::RightButton){
        pan_ += QVector3D(delta.x() * 0.0035f, -delta.y()*0.0035f, 0.f);
        update();
    }
    e->accept();
}

void SurfaceWidget::wheelEvent(QWheelEvent* e)
{
    const float num = e->angleDelta().y() / 120.0f;
    distance_ *= std::pow(0.92f, num);
    // Keep a comfortable minimum distance so the mesh doesn't get clipped by the near plane.
    distance_ = clampf(distance_, 1.5f, 30.f);
    update();
    e->accept();
}

bool SurfaceWidget::ensureProgram()
{
    if(prog_) return true;

    prog_ = new QOpenGLShaderProgram();

    const char* vs = R"(#version 330 core
layout(location=0) in vec3 a_pos;
layout(location=1) in vec3 a_nrm;
layout(location=2) in vec3 a_col;

uniform mat4 u_mvp;
out vec3 v_nrm;
out vec3 v_col;

void main(){
    gl_Position = u_mvp * vec4(a_pos, 1.0);
    v_nrm = a_nrm;
    v_col = a_col;
}
)";

    const char* fs = R"(#version 330 core
in vec3 v_nrm;
in vec3 v_col;

uniform vec3 u_lightDir;
out vec4 frag;

void main(){
    vec3 N = normalize(v_nrm);
    float diff = max(dot(N, normalize(u_lightDir)), 0.0);
    float amb = 0.28;
    vec3 col = v_col * (amb + 0.85*diff);
    frag = vec4(col, 1.0);
}
)";

    if(!prog_->addShaderFromSourceCode(QOpenGLShader::Vertex, vs)){
        return false;
    }
    if(!prog_->addShaderFromSourceCode(QOpenGLShader::Fragment, fs)){
        return false;
    }
    if(!prog_->link()){
        return false;
    }
    return true;
}

void SurfaceWidget::clearGL()
{
    if(vao_){ glDeleteVertexArrays(1, &vao_); vao_=0; }
    if(vbo_){ glDeleteBuffers(1, &vbo_); vbo_=0; }
    if(ebo_){ glDeleteBuffers(1, &ebo_); ebo_=0; }

    if(prog_){ delete prog_; prog_=nullptr; }
}

void SurfaceWidget::buildMeshCPU()
{
    vertices_.clear();
    indices_.clear();
    if(gridN_ < 3) return;

    const int N = gridN_;
    vertices_.resize(static_cast<size_t>(N*N));
    indices_.reserve(static_cast<size_t>((N-1)*(N-1)*6));

    std::vector<double> x(static_cast<size_t>(dim_), 0.0);
    if(static_cast<int>(fixed_.size())==dim_) x = fixed_;
    else x.assign(static_cast<size_t>(dim_), 0.0);

    const double loX = lower_[static_cast<size_t>(xAxis_)];
    const double hiX = upper_[static_cast<size_t>(xAxis_)];
    const double loY = lower_[static_cast<size_t>(yAxis_)];
    const double hiY = upper_[static_cast<size_t>(yAxis_)];

    // First pass: evaluate z and find min/max
    std::vector<double> zs(static_cast<size_t>(N*N), 0.0);
    double zMin= std::numeric_limits<double>::infinity();
    double zMax=-std::numeric_limits<double>::infinity();

    for(int j=0;j<N;j++){
        const double ty = double(j)/(N-1);
        const double yv = loY + (hiY-loY)*ty;
        for(int i=0;i<N;i++){
            const double tx = double(i)/(N-1);
            const double xv = loX + (hiX-loX)*tx;

            x[static_cast<size_t>(xAxis_)] = xv;
            x[static_cast<size_t>(yAxis_)] = yv;

            double z = obj_.evaluate(x);
            if(!std::isfinite(z)) z = 0.0;

            // tame extremes to keep mesh readable
            if(std::fabs(z) > 1e12) z = (z>0?1e12:-1e12);

            const size_t idx = static_cast<size_t>(j*N+i);
            zs[idx]=z;
            if(z<zMin) zMin=z;
            if(z>zMax) zMax=z;
        }
    }

    if(!std::isfinite(zMin) || !std::isfinite(zMax) || zMax==zMin){
        zMin = 0.0; zMax = 1.0;
    }
    zMin_ = static_cast<float>(zMin);
    zMax_ = static_cast<float>(zMax);

    const double zMid = 0.5*(zMin+zMax);
    const double zRange = (zMax - zMin);

    // Build vertex positions & initial colors; normals will be computed later
    for(int j=0;j<N;j++){
        const float fy = float(j)/(N-1);
        const float py = (fy*2.f - 1.f);

        for(int i=0;i<N;i++){
            const float fx = float(i)/(N-1);
            const float px = (fx*2.f - 1.f);

            const size_t idx = static_cast<size_t>(j*N+i);
            const double z0 = zs[idx];
            float pz = float((z0 - zMid) / zRange); // -0.5..0.5 roughly
            pz *= float(zScale_) * 1.8f; // emphasize but controllable

            // Color ramp based on normalized height
            float t = float((z0 - zMin) / (zMax - zMin)); // 0..1
            t = clampf(t, 0.f, 1.f);

            // perceptual-ish ramp: blue -> green -> yellow
            float r = clampf(1.4f*(t-0.5f), 0.f, 1.f);
            float g = clampf(1.2f*(1.f-std::fabs(2.f*t-1.f)), 0.f, 1.f);
            float b = clampf(1.0f - 1.2f*t, 0.f, 1.f);

            Vertex v;
            v.px=px; v.py=py; v.pz=pz;
            v.nx=0; v.ny=0; v.nz=1;
            v.r=r; v.g=g; v.b=b;
            vertices_[idx]=v;
        }
    }

    // Indices (two triangles per cell)
    for(int j=0;j<N-1;j++){
        for(int i=0;i<N-1;i++){
            const unsigned int i0 = static_cast<unsigned int>(j*N + i);
            const unsigned int i1 = static_cast<unsigned int>(j*N + (i+1));
            const unsigned int i2 = static_cast<unsigned int>((j+1)*N + i);
            const unsigned int i3 = static_cast<unsigned int>((j+1)*N + (i+1));
            // tri1: i0 i2 i1
            indices_.push_back(i0); indices_.push_back(i2); indices_.push_back(i1);
            // tri2: i1 i2 i3
            indices_.push_back(i1); indices_.push_back(i2); indices_.push_back(i3);
        }
    }

    // Compute normals by accumulating triangle normals
    std::vector<QVector3D> acc(vertices_.size(), QVector3D(0,0,0));
    for(size_t k=0;k<indices_.size();k+=3){
        const unsigned int a=indices_[k], b=indices_[k+1], c=indices_[k+2];
        const QVector3D pa(vertices_[a].px, vertices_[a].py, vertices_[a].pz);
        const QVector3D pb(vertices_[b].px, vertices_[b].py, vertices_[b].pz);
        const QVector3D pc(vertices_[c].px, vertices_[c].py, vertices_[c].pz);
        const QVector3D n = QVector3D::crossProduct(pb-pa, pc-pa);
        acc[a] += n; acc[b] += n; acc[c] += n;
    }
    for(size_t i=0;i<vertices_.size();i++){
        QVector3D n = acc[i];
        if(n.lengthSquared() < 1e-12f) n = QVector3D(0,0,1);
        n.normalize();
        vertices_[i].nx = n.x();
        vertices_[i].ny = n.y();
        vertices_[i].nz = n.z();
    }
}

void SurfaceWidget::uploadMeshGL()
{
    if(vertices_.empty() || indices_.empty()) return;

    if(vao_==0){
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);
    }

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size()*sizeof(Vertex)),
                 vertices_.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices_.size()*sizeof(unsigned int)),
                 indices_.data(), GL_STATIC_DRAW);

    // layout: position, normal, color
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, px));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));

    glBindVertexArray(0);
}
