#pragma once

#include <QWidget>
#include <QImage>
#include <QVector>

class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PlotWidget(QWidget* parent = nullptr);

    void clear();

    void setLineData(const QVector<double>& xs,
                     const QVector<double>& ys,
                     const QString& xLabel,
                     const QString& yLabel,
                     const QString& title);

    void setHeatmapData(const QImage& img,
                        const QVector<double>& gridValues,
                        int gridW,
                        int gridH,
                        double xMin, double xMax,
                        double yMin, double yMax,
                        double fMin, double fMax,
                        const QString& xLabel,
                        const QString& yLabel,
                        const QString& title);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void leaveEvent(QEvent* ev) override;

private:
    enum class Mode { None, Line, Heatmap };

    void drawAxes(QPainter& p, const QRectF& pr, const QString& xLabel, const QString& yLabel);
    void drawTicks(QPainter& p, const QRectF& pr, double xMin, double xMax, double yMin, double yMax);

private:
    Mode mode_ = Mode::None;

    // Line
    QVector<double> xs_;
    QVector<double> ys_;
    double lineXMin_ = 0.0, lineXMax_ = 1.0;
    double lineYMin_ = 0.0, lineYMax_ = 1.0;

    // Heatmap
    QImage heatmap_;
    QVector<double> grid_;
    int gridW_ = 0, gridH_ = 0;
    double hmXMin_ = 0.0, hmXMax_ = 1.0;
    double hmYMin_ = 0.0, hmYMax_ = 1.0;
    double hmFMin_ = 0.0, hmFMax_ = 1.0;

    QString xLabel_;
    QString yLabel_;
    QString title_;

    QRectF lastPlotRect_;
};
