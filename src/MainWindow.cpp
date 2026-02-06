#include "MainWindow.h"
#include <QStatusBar>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QHeaderView>
#include <QSplitter>
#include <cmath>

MainWindow::MainWindow(QWidget* parent): QMainWindow(parent)
{
    buildUi();
    populatePresets();
    presetBox_->setCurrentIndex(0);
    onPresetChanged(0);

    setMinimumSize(1200, 720);
}

void MainWindow::buildUi()
{
    auto* central = new QWidget(this);
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0,0,0,0);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    root->addWidget(splitter);

    // Left controls
    auto* left = new QWidget(splitter);
    left->setMinimumWidth(420);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(14,14,14,14);
    leftLayout->setSpacing(10);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft);

    presetBox_ = new QComboBox(left);
    connect(presetBox_, &QComboBox::currentIndexChanged, this, &MainWindow::onPresetChanged);

    exprEdit_ = new QLineEdit(left);

    dimSpin_ = new QSpinBox(left);
    dimSpin_->setMinimum(1);
    dimSpin_->setMaximum(30);
    connect(dimSpin_, &QSpinBox::valueChanged, this, &MainWindow::onDimensionChanged);

    form->addRow("Preset", presetBox_);
    form->addRow("Expression", exprEdit_);
    form->addRow("Dimension", dimSpin_);

    auto* axesBox = new QGroupBox("Axes", left);
    auto* axesForm = new QFormLayout(axesBox);

    xAxisBox_ = new QComboBox(axesBox);
    yAxisBox_ = new QComboBox(axesBox);
    connect(xAxisBox_, &QComboBox::currentIndexChanged, this, &MainWindow::onAxesChanged);
    connect(yAxisBox_, &QComboBox::currentIndexChanged, this, &MainWindow::onAxesChanged);

    axesForm->addRow("X axis", xAxisBox_);
    axesForm->addRow("Y axis", yAxisBox_);

    gridSpin_ = new QSpinBox(left);
    gridSpin_->setRange(21, 401);
    gridSpin_->setSingleStep(10);
    gridSpin_->setValue(81);
    connect(gridSpin_, &QSpinBox::valueChanged, this, &MainWindow::onGridChanged);

    wireCheck_ = new QCheckBox("Wireframe", left);
    connect(wireCheck_, &QCheckBox::stateChanged, this, &MainWindow::onWireframeChanged);

    zScale_ = new QSlider(Qt::Horizontal, left);
    zScale_->setRange(1, 400); // maps to 0.01..4.00
    zScale_->setValue(100);
    connect(zScale_, &QSlider::valueChanged, this, &MainWindow::onZScaleChanged);
    zScaleLabel_ = new QLabel("Z scale: 1.00", left);

    leftLayout->addLayout(form);
    leftLayout->addWidget(axesBox);

    auto* gridBox = new QGroupBox("Sampling", left);
    auto* gridForm = new QFormLayout(gridBox);
    gridForm->addRow("Grid N×N", gridSpin_);
    gridForm->addRow("", wireCheck_);
    gridForm->addRow("", zScaleLabel_);
    gridForm->addRow("", zScale_);
    leftLayout->addWidget(gridBox);

    auto* tableBox = new QGroupBox("Per-variable bounds / fixed values", left);
    auto* tableLay = new QVBoxLayout(tableBox);
    table_ = new QTableWidget(tableBox);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({"Var","Lower","Upper","Fixed"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setAlternatingRowColors(true);
    tableLay->addWidget(table_);
    leftLayout->addWidget(tableBox, 1);

    applyBtn_ = new QPushButton("Apply / Rebuild", left);
    connect(applyBtn_, &QPushButton::clicked, this, &MainWindow::onApply);
    leftLayout->addWidget(applyBtn_);

    splitter->addWidget(left);

    // Right: surface
    surface_ = new SurfaceWidget(splitter);
    splitter->addWidget(surface_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    setCentralWidget(central);
    statusBar()->showMessage("Ready.");
}

void MainWindow::populatePresets()
{
    presets_.clear();

    auto add = [&](const char* name, const QString& expr, int dim, double lo, double hi){
        presets_.push_back({QString::fromLatin1(name), expr, dim, lo, hi});
    };

    auto makeWeierstrass2D = []()->QString{
        // Standard Weierstrass with a=0.5, b=3, kmax=20 (2D). Domain typically [-0.5, 0.5].
        // f(x) = sum_i sum_k a^k cos(2*pi*b^k*(x_i+0.5)) - n*sum_k a^k cos(2*pi*b^k*0.5)
        const int kmax = 20;
        const double a = 0.5;
        const double b = 3.0;
        auto termForX = [&](const QString& xi)->QString{
            QString s;
            for(int k=0;k<=kmax;k++){
                const double ak = std::pow(a, k);
                const double bk = std::pow(b, k);
                if(k) s += " + ";
                s += QString("%1*cos(2*pi*%2*(%3+0.5))").arg(ak,0,'g',17).arg(bk,0,'g',17).arg(xi);
            }
            return "(" + s + ")";
        };

        QString csum;
        for(int k=0;k<=kmax;k++){
            const double ak = std::pow(a, k);
            const double bk = std::pow(b, k);
            if(k) csum += " + ";
            csum += QString("%1*cos(2*pi*%2*0.5)").arg(ak,0,'g',17).arg(bk,0,'g',17);
        }
        const QString base = termForX("x0") + " + " + termForX("x1");
        return base + QString(" - 2*(%1)").arg(csum);
    };

    auto makeHartmann3 = []()->QString{
        // Hartmann 3D (classic definition), domain [0,1]^3.
        // f(x) = -sum_{i=1..4} alpha_i * exp(-sum_{j=1..3} A_ij*(x_j - P_ij)^2)
        const double alpha[4] = {1.0, 1.2, 3.0, 3.2};
        const double A[4][3] = {
            {3.0, 10.0, 30.0},
            {0.1, 10.0, 35.0},
            {3.0, 10.0, 30.0},
            {0.1, 10.0, 35.0}
        };
        const double P[4][3] = {
            {0.3689, 0.1170, 0.2673},
            {0.4699, 0.4387, 0.7470},
            {0.1091, 0.8732, 0.5547},
            {0.0381, 0.5743, 0.8828}
        };

        QString s;
        for(int i=0;i<4;i++){
            QString inner;
            for(int j=0;j<3;j++){
                if(j) inner += " + ";
                inner += QString("%1*(x%2-%3)^2").arg(A[i][j],0,'g',17).arg(j).arg(P[i][j],0,'g',17);
            }
            const QString term = QString("%1*exp(-(%2))").arg(alpha[i],0,'g',17).arg(inner);
            if(i) s += " + ";
            s += term;
        }
        return "- (" + s + ")";
    };

    auto makeHartmann6 = []()->QString{
        // Hartmann 6D (classic definition), domain [0,1]^6.
        const double alpha[4] = {1.0, 1.2, 3.0, 3.2};
        const double A[4][6] = {
            {10.0, 3.0, 17.0, 3.5, 1.7, 8.0},
            {0.05, 10.0, 17.0, 0.1, 8.0, 14.0},
            {3.0, 3.5, 1.7, 10.0, 17.0, 8.0},
            {17.0, 8.0, 0.05, 10.0, 0.1, 14.0}
        };
        const double P[4][6] = {
            {0.1312, 0.1696, 0.5569, 0.0124, 0.8283, 0.5886},
            {0.2329, 0.4135, 0.8307, 0.3736, 0.1004, 0.9991},
            {0.2348, 0.1451, 0.3522, 0.2883, 0.3047, 0.6650},
            {0.4047, 0.8828, 0.8732, 0.5743, 0.1091, 0.0381}
        };

        QString s;
        for(int i=0;i<4;i++){
            QString inner;
            for(int j=0;j<6;j++){
                if(j) inner += " + ";
                inner += QString("%1*(x%2-%3)^2").arg(A[i][j],0,'g',17).arg(j).arg(P[i][j],0,'g',17);
            }
            const QString term = QString("%1*exp(-(%2))").arg(alpha[i],0,'g',17).arg(inner);
            if(i) s += " + ";
            s += term;
        }
        return "- (" + s + ")";
    };

    auto makeShekel = [](int m)->QString{
        // Shekel family (m=5,7,10), 4D, domain [0,10]^4.
        const double A[10][4] = {
            {4.0, 4.0, 4.0, 4.0},
            {1.0, 1.0, 1.0, 1.0},
            {8.0, 8.0, 8.0, 8.0},
            {6.0, 6.0, 6.0, 6.0},
            {3.0, 7.0, 3.0, 7.0},
            {2.0, 9.0, 2.0, 9.0},
            {5.0, 5.0, 3.0, 3.0},
            {8.0, 1.0, 8.0, 1.0},
            {6.0, 2.0, 6.0, 2.0},
            {7.0, 3.6, 7.0, 3.6}
        };
        const double C[10] = {0.1,0.2,0.2,0.4,0.4,0.6,0.3,0.7,0.5,0.5};

        QString s;
        for(int i=0;i<m;i++){
            QString denom;
            for(int j=0;j<4;j++){
                if(j) denom += " + ";
                denom += QString("(x%1-%2)^2").arg(j).arg(A[i][j],0,'g',17);
            }
            denom += QString(" + %1").arg(C[i],0,'g',17);
            const QString term = QString("1/(%1)").arg(denom);
            if(i) s += " + ";
            s += term;
        }
        return "- (" + s + ")";
    };

    // Analytic presets (supported by the expression parser)
    add("rastrigin",
        QStringLiteral("20 + (x0^2 - 10*cos(2*pi*x0)) + (x1^2 - 10*cos(2*pi*x1))"), 2, -5.12, 5.12);
    add("rosenbrock",
        QStringLiteral("(1 - x0)^2 + 100*(x1 - x0^2)^2"), 2, -2.048, 2.048);

    // Framework-specific / not representable as a single analytic string (placeholder in standalone mode)
    add("potential", QString(), 2, -5.0, 5.0);

    add("ackley",
        QStringLiteral("-20*exp(-0.2*sqrt(0.5*(x0^2+x1^2))) - exp(0.5*(cos(2*pi*x0)+cos(2*pi*x1))) + 20 + e"), 2, -32.768, 32.768);
    add("sphere",
        QStringLiteral("x0^2 + x1^2"), 2, -5.12, 5.12);
    add("griewank",
        QStringLiteral("1 + (x0^2 + x1^2)/4000 - cos(x0)*cos(x1/sqrt(2))"), 2, -600.0, 600.0);

    // Levy N.13 (2D)
    add("levy",
        QStringLiteral("(sin(3*pi*x0))^2 + (x0-1)^2*(1 + (sin(3*pi*x1))^2) + (x1-1)^2*(1 + (sin(2*pi*x1))^2)"), 2, -10.0, 10.0);

    add("attractivesector", QString(), 2, -5.0, 5.0);

    add("bohachevsky1",
        QStringLiteral("x0^2 + 2*x1^2 - 0.3*cos(3*pi*x0) - 0.4*cos(4*pi*x1) + 0.7"), 2, -100.0, 100.0);
    add("bohachevsky2",
        QStringLiteral("x0^2 + 2*x1^2 - 0.3*cos(3*pi*x0)*cos(4*pi*x1) + 0.3"), 2, -100.0, 100.0);
    add("bohachevsky3",
        QStringLiteral("x0^2 + 2*x1^2 - 0.3*cos(3*pi*x0 + 4*pi*x1) + 0.3"), 2, -100.0, 100.0);

    // Branin (classic bounds are per-variable; here an envelope)
    add("branin",
        QStringLiteral("(x1 - (5.1/(4*pi^2))*x0^2 + (5/pi)*x0 - 6)^2 + 10*(1 - 1/(8*pi))*cos(x0) + 10"), 2, -5.0, 15.0);

    // Six-hump camel (classic bounds are per-variable; here an envelope)
    add("camel",
        QStringLiteral("((4 - 2.1*x0^2 + (x0^4)/3)*x0^2) + (x0*x1) + ((-4 + 4*x1^2)*x1^2)"), 2, -3.0, 3.0);

    add("cigar",
        QStringLiteral("x0^2 + 1000000*x1^2"), 2, -100.0, 100.0);

    // Cosine Mixture (common variant)
    add("cosinemixture",
        QStringLiteral("x0^2 + x1^2 - 0.1*(cos(5*pi*x0) + cos(5*pi*x1))"), 2, -1.0, 1.0);

    add("differentpowers",
        QStringLiteral("(abs(x0))^2 + (abs(x1))^3"), 2, -1.0, 1.0);

    add("diracproblem", QString(), 2, -5.0, 5.0);

    add("easom",
        QStringLiteral("-cos(x0)*cos(x1)*exp(-((x0-pi)^2 + (x1-pi)^2))"), 2, -100.0, 100.0);

    // Ellipsoidal (2D specialization). Note: for n=2 it matches the common 1e6-conditioned ellipsoid.
    add("ellipsoidal",
        QStringLiteral("x0^2 + 1000000*x1^2"), 2, -5.0, 5.0);

    add("equalmaxima",
        QStringLiteral("(sin(5*pi*x0))^6"), 2, 0.0, 1.0);

    // Exponential (common benchmark): f(x) = -exp(-0.5*sum x_i^2)
    add("expotential",
        QStringLiteral("-exp(-0.5*(x0^2+x1^2))"), 2, -1.0, 1.0);

    add("goldstein",
        QStringLiteral(
            "(1 + (x0 + x1 + 1)^2*(19 - 14*x0 + 3*x0^2 - 14*x1 + 6*x0*x1 + 3*x1^2))"
            " * (30 + (2*x0 - 3*x1)^2*(18 - 32*x0 + 12*x0^2 + 48*x1 - 36*x0*x1 + 27*x1^2))"
        ),
        2, -2.0, 2.0);

    // Griewank-Rosenbrock (F8F2, 2D specialization)
    add("griewankrosenbrock",
        QStringLiteral("(pow(100*(x0^2 - x1)^2 + (x0-1)^2,2)/4000) - cos(100*(x0^2 - x1)^2 + (x0-1)^2) + 1"),
        2, -5.0, 5.0);

    // Hansen is not a single canonical definition across benchmark suites; keep placeholder in standalone mode.
    add("hansen", QString(), 2, -5.0, 5.0);

    add("hartmann3", makeHartmann3(), 3, 0.0, 1.0);
    add("hartmann6", makeHartmann6(), 6, 0.0, 1.0);

    // Variants typically involve shifting/rotation in their canonical definitions.
    // In standalone mode, they are kept as placeholders unless you provide the exact variant definition.
    add("rastrigin2", QString(), 2, -5.12, 5.12);
    add("rotatedrosenbrock", QString(), 2, -2.048, 2.048);

    add("shekel5",  makeShekel(5), 4, 0.0, 10.0);
    add("shekel7",  makeShekel(7), 4, 0.0, 10.0);
    add("shekel10", makeShekel(10), 4, 0.0, 10.0);

    add("shubert",
        QStringLiteral(
            "(cos(2*x0 + 1) + 2*cos(3*x0 + 2) + 3*cos(4*x0 + 3) + 4*cos(5*x0 + 4) + 5*cos(6*x0 + 5))"
            " * (cos(2*x1 + 1) + 2*cos(3*x1 + 2) + 3*cos(4*x1 + 3) + 4*cos(5*x1 + 4) + 5*cos(6*x1 + 5))"
        ),
        2, -10.0, 10.0);

    // Step-Ellipsoidal (2D specialization) using floor(x+0.5)
    add("stepellipsoidal",
        QStringLiteral("floor(x0+0.5)^2 + 1000000*floor(x1+0.5)^2"), 2, -5.0, 5.0);
    add("test2n", QString(), 2, -5.0, 5.0);
    add("test30n", QString(), 30, -5.0, 5.0);

    add("antennaarray", QString(), 2, -5.0, 5.0);
    add("antennaula", QString(), 2, -5.0, 5.0);
    add("bifunctionalcatalyst", QString(), 2, -5.0, 5.0);
    add("bucherastrigin", QString(), 2, -5.0, 5.0);
    add("cassini", QString(), 2, -5.0, 5.0);
    add("ded1", QString(), 2, -5.0, 5.0);
    add("ded2", QString(), 2, -5.0, 5.0);
    add("eld1", QString(), 2, -5.0, 5.0);
    add("eld2", QString(), 2, -5.0, 5.0);
    add("eld3", QString(), 2, -5.0, 5.0);
    add("eld4", QString(), 2, -5.0, 5.0);
    add("eld5", QString(), 2, -5.0, 5.0);
    add("fmsynth", QString(), 2, -5.0, 5.0);
    add("gallagher101", QString(), 2, -5.0, 5.0);
    add("gallagher21", QString(), 2, -5.0, 5.0);
    add("heatexchanger", QString(), 2, -5.0, 5.0);

    add("himmelblau",
        QStringLiteral("(x0^2 + x1 - 11)^2 + (x0 + x1^2 - 7)^2"), 2, -5.0, 5.0);

    add("hydrothermal", QString(), 2, -5.0, 5.0);
    add("ik6dof", QString(), 2, -5.0, 5.0);
    add("katsuura", QString(), 2, -5.0, 5.0);
    add("lunacekbirastrigin", QString(), 2, -5.0, 5.0);
    add("messenger", QString(), 2, -5.0, 5.0);

    // Michalewicz (2D, m=10) - using a common 2D specialization
    add("michalewicz",
        QStringLiteral("-(sin(x0) * (sin(1*x0^2/pi))^20 + sin(x1) * (sin(2*x1^2/pi))^20)"), 2, 0.0, 3.141592653589793);

    add("ofdmpower", QString(), 2, -5.0, 5.0);
    add("polyphase", QString(), 2, -5.0, 5.0);
    add("portfoliomv", QString(), 2, -5.0, 5.0);

    // Schaffer N.2
    add("schaffer",
        QStringLiteral("0.5 + ((sin(x0^2 - x1^2))^2 - 0.5) / (1 + 0.001*(x0^2 + x1^2))^2"), 2, -100.0, 100.0);

    // Schwefel 2.26 (2D specialization)
    add("schwefel",
        QStringLiteral("837.9658 - (x0*sin(sqrt(abs(x0))) + x1*sin(sqrt(abs(x1))))"), 2, -500.0, 500.0);

    add("tandem", QString(), 2, -5.0, 5.0);
    add("tersoffb", QString(), 2, -5.0, 5.0);
    add("tersoffc", QString(), 2, -5.0, 5.0);
    add("tnep", QString(), 2, -5.0, 5.0);
    add("transmissionpricing", QString(), 2, -5.0, 5.0);
    add("vibratingplatform", QString(), 2, -5.0, 5.0);
    add("weierstrass", makeWeierstrass2D(), 2, -0.5, 0.5);
    add("wirelesscoverage", QString(), 2, -5.0, 5.0);

    add("zakharov",
        QStringLiteral("x0^2 + x1^2 + (0.5*(1*x0 + 2*x1))^2 + (0.5*(1*x0 + 2*x1))^4"), 2, -5.0, 10.0);

    add("sinusoidal", QString(), 2, -5.0, 5.0);
    add("gascycle", QString(), 2, -5.0, 5.0);

    add("gkls", QString(), 2, -1.0, 1.0);
    add("gkls250", QString(), 2, -1.0, 1.0);
    add("gkls350", QString(), 2, -1.0, 1.0);
    add("gkls2100", QString(), 2, -1.0, 1.0);

    presetBox_->blockSignals(true);
    presetBox_->clear();
    for(const auto& p: presets_) presetBox_->addItem(p.name);
    presetBox_->blockSignals(false);
}

void MainWindow::onPresetChanged(int idx)
{
    if(idx<0 || idx>=static_cast<int>(presets_.size())) return;
    const auto& p = presets_[static_cast<size_t>(idx)];

    exprEdit_->setText(p.expr);
    dimSpin_->setValue(p.dim);

    lower_.assign(p.dim, p.lo);
    upper_.assign(p.dim, p.hi);
    fixed_.assign(p.dim, 0.0);

    refreshAxesCombos();
    refreshBoundsTable();

    if(p.expr.trimmed().isEmpty()){
        setStatus(QString("Preset '%1' is a placeholder in standalone mode (no analytic expression). "
                          "Enter an expression manually and click Apply / Rebuild.").arg(p.name));
        return;
    }

    onApply();
}

void MainWindow::onDimensionChanged(int v)
{
    if(v<=0) return;
    lower_.assign(v, -5.0);
    upper_.assign(v, 5.0);
    fixed_.assign(v, 0.0);

    refreshAxesCombos();
    refreshBoundsTable();
}

void MainWindow::refreshAxesCombos()
{
    xAxisBox_->blockSignals(true);
    yAxisBox_->blockSignals(true);
    xAxisBox_->clear();
    yAxisBox_->clear();

    const int d = dimSpin_->value();
    for(int i=0;i<d;i++){
        const QString label = QString("x%1").arg(i);
        xAxisBox_->addItem(label, i);
        yAxisBox_->addItem(label, i);
    }
    xAxisBox_->setCurrentIndex(0);
    yAxisBox_->setCurrentIndex(d>1 ? 1 : 0);
    xAxisBox_->blockSignals(false);
    yAxisBox_->blockSignals(false);
}

void MainWindow::refreshBoundsTable()
{
    const int d = dimSpin_->value();
    table_->blockSignals(true);
    table_->setRowCount(d);
    for(int r=0;r<d;r++) setTableRow(r, r);
    table_->blockSignals(false);
}

void MainWindow::setTableRow(int row, int varIndex)
{
    auto* itVar = new QTableWidgetItem(QString("x%1").arg(varIndex));
    itVar->setFlags(Qt::ItemIsEnabled);

    auto* itLo = new QTableWidgetItem(QString::number(lower_[static_cast<size_t>(varIndex)]));
    auto* itHi = new QTableWidgetItem(QString::number(upper_[static_cast<size_t>(varIndex)]));
    auto* itFx = new QTableWidgetItem(QString::number(fixed_[static_cast<size_t>(varIndex)]));

    table_->setItem(row, 0, itVar);
    table_->setItem(row, 1, itLo);
    table_->setItem(row, 2, itHi);
    table_->setItem(row, 3, itFx);
}

bool MainWindow::readTableToVectors(std::vector<double>& lower, std::vector<double>& upper, std::vector<double>& fixed)
{
    const int d = dimSpin_->value();
    lower.assign(d, 0.0);
    upper.assign(d, 0.0);
    fixed.assign(d, 0.0);

    for(int r=0;r<d;r++){
        bool ok1=false, ok2=false, ok3=false;
        double lo = table_->item(r,1)->text().toDouble(&ok1);
        double hi = table_->item(r,2)->text().toDouble(&ok2);
        double fx = table_->item(r,3)->text().toDouble(&ok3);
        if(!ok1 || !ok2 || !ok3){
            QMessageBox::warning(this, "Invalid input", QString("Row %1 contains invalid numeric values.").arg(r));
            return false;
        }
        if(hi<=lo){
            QMessageBox::warning(this, "Invalid bounds", QString("Row %1: upper must be > lower.").arg(r));
            return false;
        }
        lower[static_cast<size_t>(r)] = lo;
        upper[static_cast<size_t>(r)] = hi;
        fixed[static_cast<size_t>(r)] = fx;
    }
    return true;
}

void MainWindow::onAxesChanged()
{
    // Rebuild immediately to make axis changes obvious.
    onApply();
}

void MainWindow::onGridChanged(int)
{
    // no automatic rebuild here; keep it responsive for large grids
}

void MainWindow::onWireframeChanged(int)
{
    surface_->setWireframe(wireCheck_->isChecked());
    surface_->update();
}

void MainWindow::onZScaleChanged(int v)
{
    const double s = static_cast<double>(v)/100.0;
    zScaleLabel_->setText(QString("Z scale: %1").arg(s, 0, 'f', 2));
    surface_->setZScale(s);
    surface_->update();
}

void MainWindow::onApply()
{
    const QString exprText = exprEdit_->text().trimmed();
    if(exprText.isEmpty()){
        setStatus("No expression to evaluate. Select an analytic preset or enter an expression manually.");
        return;
    }

    const int d = dimSpin_->value();
    std::string err;
    if(!obj_.setExpression(exprText.toStdString(), d, &err)){
        QMessageBox::critical(this, "Expression error", QString::fromStdString(err));
        return;
    }

    std::vector<double> lo, hi, fx;
    if(!readTableToVectors(lo, hi, fx)) return;
    lower_ = lo; upper_ = hi; fixed_ = fx;

    const int xAxis = xAxisBox_->currentData().toInt();
    const int yAxis = yAxisBox_->currentData().toInt();
    if(xAxis==yAxis){
        QMessageBox::warning(this, "Axes", "X axis and Y axis must be different.");
        return;
    }

    surface_->setObjective(obj_);
    surface_->setDimension(d);
    surface_->setAxes(xAxis, yAxis);
    surface_->setGridN(gridSpin_->value());
    surface_->setBounds(lower_, upper_);
    surface_->setFixed(fixed_);
    surface_->setWireframe(wireCheck_->isChecked());
    surface_->rebuildSurface();

    setStatus(QString("Rendering %1×%1 grid. Axes: x%2 vs x%3.")
                  .arg(gridSpin_->value()).arg(xAxis).arg(yAxis));
}

void MainWindow::setStatus(const QString& s)
{
    statusBar()->showMessage(s, 5000);
}
