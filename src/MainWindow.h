#pragma once
#include <QMainWindow>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include "SurfaceWidget.h"
#include "ObjectiveFunction.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget* parent=nullptr);

private slots:
    void onPresetChanged(int idx);
    void onDimensionChanged(int v);
    void onAxesChanged();
    void onApply();
    void onWireframeChanged(int state);
    void onGridChanged(int v);
    void onZScaleChanged(int v);

private:
    void buildUi();
    void populatePresets();
    void refreshAxesCombos();
    void refreshBoundsTable();
    void setTableRow(int row, int varIndex);
    bool readTableToVectors(std::vector<double>& lower, std::vector<double>& upper, std::vector<double>& fixed);
    void setStatus(const QString& s);

    struct Preset { QString name; QString expr; int dim; double lo; double hi; };
    std::vector<Preset> presets_;

    SurfaceWidget* surface_{nullptr};

    QComboBox* presetBox_{nullptr};
    QLineEdit* exprEdit_{nullptr};
    QSpinBox* dimSpin_{nullptr};
    QComboBox* xAxisBox_{nullptr};
    QComboBox* yAxisBox_{nullptr};
    QSpinBox* gridSpin_{nullptr};
    QCheckBox* wireCheck_{nullptr};
    QSlider* zScale_{nullptr};
    QLabel* zScaleLabel_{nullptr};
    QTableWidget* table_{nullptr};
    QPushButton* applyBtn_{nullptr};

    ObjectiveFunction obj_;
    std::vector<double> lower_, upper_, fixed_;
};
