#include "PlotWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QToolTip>
#include <QtMath>
#include <cmath>

static bool finite(double v) { return std::isfinite(v); }

PlotWidget::PlotWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(480, 360);
}

void PlotWidget::clear()
{
    mode_ = Mode::None;
    xs_.clear();
    ys_.clear();
    heatmap_ = QImage();
    grid_.clear();
    gridW_ = gridH_ = 0;
    xLabel_.clear();
    yLabel_.clear();
    title_.clear();
    update();
}

void PlotWidget::setLineData(const QVector<double>& xs,
                             const QVector<double>& ys,
                             const QString& xLabel,
                             const QString& yLabel,
                             const QString& title)
{
    mode_ = Mode::Line;
    xs_ = xs;
    ys_ = ys;
    xLabel_ = xLabel;
    yLabel_ = yLabel;
    title_ = title;

    if (!xs_.isEmpty())
    {
        lineXMin_ = lineXMax_ = xs_[0];
        for (double v : xs_)
        {
            if (v < lineXMin_) lineXMin_ = v;
            if (v > lineXMax_) lineXMax_ = v;
        }
    }

    bool hasY = false;
    for (double v : ys_)
    {
        if (finite(v))
        {
            if (!hasY) { lineYMin_ = lineYMax_ = v; hasY = true; }
            else { if (v < lineYMin_) lineYMin_ = v; if (v > lineYMax_) lineYMax_ = v; }
        }
    }
    if (!hasY) { lineYMin_ = -1.0; lineYMax_ = 1.0; }
    if (lineYMin_ == lineYMax_) { lineYMin_ -= 1.0; lineYMax_ += 1.0; }

    update();
}

void PlotWidget::setHeatmapData(const QImage& img,
                                const QVector<double>& gridValues,
                                int gridW,
                                int gridH,
                                double xMin, double xMax,
                                double yMin, double yMax,
                                double fMin, double fMax,
                                const QString& xLabel,
                                const QString& yLabel,
                                const QString& title)
{
    mode_ = Mode::Heatmap;
    heatmap_ = img;
    grid_ = gridValues;
    gridW_ = gridW;
    gridH_ = gridH;

    hmXMin_ = xMin; hmXMax_ = xMax;
    hmYMin_ = yMin; hmYMax_ = yMax;
    hmFMin_ = fMin; hmFMax_ = fMax;

    xLabel_ = xLabel;
    yLabel_ = yLabel;
    title_ = title;

    update();
}

void PlotWidget::drawAxes(QPainter& p, const QRectF& pr, const QString& xLabel, const QString& yLabel)
{
    p.save();
    p.setPen(QPen(Qt::black, 1));
    p.drawRect(pr);

    // Title
    QFont f = p.font();
    f.setPointSizeF(std::max(10.0, f.pointSizeF() + 1.5));
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(0, 0, width(), pr.top()), Qt::AlignCenter, title_);

    // Axis labels
    f.setBold(false);
    f.setPointSizeF(std::max(9.0, f.pointSizeF() - 1.0));
    p.setFont(f);

    p.drawText(QRectF(pr.left(), pr.bottom() + 8, pr.width(), 30), Qt::AlignCenter, xLabel);

    p.save();
    p.translate(12, pr.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-pr.height()/2.0, -30, pr.height(), 30), Qt::AlignCenter, yLabel);
    p.restore();

    p.restore();
}

void PlotWidget::drawTicks(QPainter& p, const QRectF& pr, double xMin, double xMax, double yMin, double yMax)
{
    p.save();
    p.setPen(QPen(Qt::black, 1));
    QFont f = p.font();
    f.setPointSizeF(std::max(8.0, f.pointSizeF() - 1.0));
    p.setFont(f);

    const int ticks = 5;
    for (int i = 0; i < ticks; ++i)
    {
        const double t = (ticks == 1) ? 0.0 : (double(i) / double(ticks - 1));
        const double xv = xMin + t * (xMax - xMin);
        const double px = pr.left() + t * pr.width();
        p.drawLine(QPointF(px, pr.bottom()), QPointF(px, pr.bottom() + 6));
        p.drawText(QRectF(px - 40, pr.bottom() + 8, 80, 18), Qt::AlignHCenter | Qt::AlignTop, QString::number(xv, 'g', 6));
    }

    for (int i = 0; i < ticks; ++i)
    {
        const double t = (ticks == 1) ? 0.0 : (double(i) / double(ticks - 1));
        const double yv = yMin + (1.0 - t) * (yMax - yMin);
        const double py = pr.top() + t * pr.height();
        p.drawLine(QPointF(pr.left() - 6, py), QPointF(pr.left(), py));
        p.drawText(QRectF(0, py - 9, pr.left() - 10, 18), Qt::AlignRight | Qt::AlignVCenter, QString::number(yv, 'g', 6));
    }

    p.restore();
}

void PlotWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), Qt::white);

    const int left = 70;
    const int right = 80;
    const int top = 40;
    const int bottom = 55;

    QRectF pr(left, top, width() - left - right, height() - top - bottom);
    lastPlotRect_ = pr;

    if (mode_ == Mode::None)
    {
        p.setPen(Qt::black);
        p.drawText(rect(), Qt::AlignCenter, "No plot");
        return;
    }

    drawAxes(p, pr, xLabel_, yLabel_);

    if (mode_ == Mode::Line)
    {
        drawTicks(p, pr, lineXMin_, lineXMax_, lineYMin_, lineYMax_);

        // Draw line
        if (xs_.size() >= 2 && ys_.size() == xs_.size())
        {
            QPainterPath path;
            bool started = false;

            for (int i = 0; i < xs_.size(); ++i)
            {
                const double xv = xs_[i];
                const double yv = ys_[i];
                if (!finite(yv)) continue;

                const double tx = (lineXMax_ == lineXMin_) ? 0.0 : ((xv - lineXMin_) / (lineXMax_ - lineXMin_));
                const double ty = (lineYMax_ == lineYMin_) ? 0.0 : ((yv - lineYMin_) / (lineYMax_ - lineYMin_));
                const double px = pr.left() + tx * pr.width();
                const double py = pr.bottom() - ty * pr.height();

                if (!started)
                {
                    path.moveTo(px, py);
                    started = true;
                }
                else
                {
                    path.lineTo(px, py);
                }
            }

            p.save();
            p.setPen(QPen(Qt::darkBlue, 2));
            p.drawPath(path);
            p.restore();
        }

        return;
    }

    // Heatmap
    if (!heatmap_.isNull())
    {
        drawTicks(p, pr, hmXMin_, hmXMax_, hmYMin_, hmYMax_);

        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(pr, heatmap_);
        p.restore();

        // Color legend
        const QRectF legendRect(width() - 60, pr.top(), 18, pr.height());
        QLinearGradient grad(legendRect.topLeft(), legendRect.bottomLeft());
        grad.setColorAt(0.0, QColor::fromHsv(0, 255, 255));
        grad.setColorAt(1.0, QColor::fromHsv(240, 255, 255));
        p.fillRect(legendRect, grad);
        p.setPen(Qt::black);
        p.drawRect(legendRect);

        QFont f = p.font();
        f.setPointSizeF(std::max(8.0, f.pointSizeF() - 1.0));
        p.setFont(f);

        p.drawText(QRectF(legendRect.right() + 4, legendRect.top() - 8, 55, 18),
                   Qt::AlignLeft | Qt::AlignVCenter, QString::number(hmFMax_, 'g', 6));
        p.drawText(QRectF(legendRect.right() + 4, legendRect.bottom() - 10, 55, 18),
                   Qt::AlignLeft | Qt::AlignVCenter, QString::number(hmFMin_, 'g', 6));
    }
}

void PlotWidget::mouseMoveEvent(QMouseEvent* ev)
{
    if (mode_ != Mode::Heatmap || heatmap_.isNull() || gridW_ <= 1 || gridH_ <= 1)
    {
        QWidget::mouseMoveEvent(ev);
        return;
    }

    const QPointF pt = ev->pos();
    if (!lastPlotRect_.contains(pt))
    {
        QToolTip::hideText();
        QWidget::mouseMoveEvent(ev);
        return;
    }

    const double u = (pt.x() - lastPlotRect_.left()) / lastPlotRect_.width();
    const double v = (pt.y() - lastPlotRect_.top()) / lastPlotRect_.height();

    const int ix = int(std::round(u * double(gridW_ - 1)));
    const int iy = int(std::round((1.0 - v) * double(gridH_ - 1)));

    if (ix < 0 || ix >= gridW_ || iy < 0 || iy >= gridH_)
    {
        QToolTip::hideText();
        QWidget::mouseMoveEvent(ev);
        return;
    }

    const double x = hmXMin_ + (double(ix) / double(gridW_ - 1)) * (hmXMax_ - hmXMin_);
    const double y = hmYMin_ + (double(iy) / double(gridH_ - 1)) * (hmYMax_ - hmYMin_);
    const double f = grid_[iy * gridW_ + ix];

    const QString text = QString("%1=%2\n%3=%4\nf=%5")
        .arg(xLabel_).arg(x, 0, 'g', 8)
        .arg(yLabel_).arg(y, 0, 'g', 8)
        .arg(f, 0, 'g', 10);

    QToolTip::showText(ev->globalPos(), text, this);
    QWidget::mouseMoveEvent(ev);
}

void PlotWidget::leaveEvent(QEvent* ev)
{
    QToolTip::hideText();
    QWidget::leaveEvent(ev);
}
