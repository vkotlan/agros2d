// This file is part of Agros2D.
//
// Agros2D is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// Agros2D is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Agros2D.  If not, see <http://www.gnu.org/licenses/>.
//
// hp-FEM group (http://hpfem.org/)
// University of Nevada, Reno (UNR) and University of West Bohemia, Pilsen
// Email: agros2d@googlegroups.com, home page: http://hpfem.org/agros2d/

#include "problemdialog.h"

#include "util/global.h"

#include "scene.h"
#include "moduledialog.h"
#include "pythonlab/pythonengine_agros.h"

#include "hermes2d/module.h"
#include "hermes2d/module_agros.h"
#include "hermes2d/coupling.h"
#include "hermes2d/problem_config.h"

#include "gui/lineeditdouble.h"
#include "gui/latexviewer.h"
#include "gui/common.h"

const int minWidth = 130;

FieldSelectDialog::FieldSelectDialog(QList<QString> fields, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Select field"));
    setModal(true);

    m_selectedFieldId = "";

    lstFields = new QListWidget(this);
    lstFields->setIconSize(QSize(32, 32));
    lstFields->setMinimumHeight(36*8);

    QMapIterator<QString, QString> it(availableModules());
    while (it.hasNext())
    {
        it.next();

        // add only missing fields
        if (!fields.contains(it.key()))
        {
            QListWidgetItem *item = new QListWidgetItem(lstFields);
            item->setIcon(icon("fields/" + it.key()));
            item->setText(it.value());
            item->setData(Qt::UserRole, it.key());

            lstFields->addItem(item);
        }
    }

    connect(lstFields, SIGNAL(itemDoubleClicked(QListWidgetItem *)),
            this, SLOT(doItemDoubleClicked(QListWidgetItem *)));
    connect(lstFields, SIGNAL(itemActivated(QListWidgetItem *)),
            this, SLOT(doItemSelected(QListWidgetItem *)));
    connect(lstFields, SIGNAL(itemPressed(QListWidgetItem *)),
            this, SLOT(doItemSelected(QListWidgetItem *)));

    QGridLayout *layoutSurface = new QGridLayout();
    layoutSurface->addWidget(lstFields);

    QWidget *widget = new QWidget();
    widget->setLayout(layoutSurface);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(doAccept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(doReject()));

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(widget, 1);
    layout->addStretch();
    layout->addWidget(buttonBox);

    setLayout(layout);

    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    if (lstFields->count() > 0)
    {
        lstFields->setCurrentRow(0);
        doItemSelected(lstFields->currentItem());
    }
}

void FieldSelectDialog::doAccept()
{
    accept();
}

void FieldSelectDialog::doReject()
{
    reject();
}

int FieldSelectDialog::showDialog()
{
    return exec();
}

void FieldSelectDialog::doItemSelected(QListWidgetItem *item)
{
    m_selectedFieldId = item->data(Qt::UserRole).toString();
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
}

void FieldSelectDialog::doItemDoubleClicked(QListWidgetItem *item)
{
    if (lstFields->currentItem())
    {
        m_selectedFieldId = lstFields->currentItem()->data(Qt::UserRole).toString();
        accept();
    }
}

// ********************************************************************************************************

FieldWidget::FieldWidget(FieldInfo *fieldInfo, QWidget *parent)
    : QWidget(parent), m_fieldInfo(fieldInfo)
{
    createContent();
    load();
}

void FieldWidget::createContent()
{
    // equations
    equationLaTeX = new LaTeXViewer(this);
    equationLaTeX->setMaximumWidth(400);

    cmbAdaptivityType = new QComboBox();
    txtAdaptivitySteps = new QSpinBox(this);
    txtAdaptivitySteps->setMinimum(1);
    txtAdaptivitySteps->setMaximum(100);
    txtAdaptivityTolerance = new LineEditDouble(1, true);
    txtAdaptivityTolerance->setBottom(0.0);
    txtAdaptivityBackSteps = new QSpinBox(this);
    txtAdaptivityBackSteps->setMinimum(0);
    txtAdaptivityBackSteps->setMaximum(100);
    txtAdaptivityRedoneEach = new QSpinBox(this);
    txtAdaptivityRedoneEach->setMinimum(1);
    txtAdaptivityRedoneEach->setMaximum(100);

    // mesh
    txtNumberOfRefinements = new QSpinBox(this);
    txtNumberOfRefinements->setMinimum(0);
    txtNumberOfRefinements->setMaximum(5);
    txtPolynomialOrder = new QSpinBox(this);
    txtPolynomialOrder->setMinimum(1);
    txtPolynomialOrder->setMaximum(10);

    // transient
    cmbAnalysisType = new QComboBox();
    txtTransientInitialCondition = new ValueLineEdit();
    txtTransientTimeSkip = new ValueLineEdit();
    txtTransientTimeSkip->setMinimum(0);

    // linearity
    cmbLinearityType = new QComboBox();
    txtNonlinearSteps = new QSpinBox(this);
    txtNonlinearSteps->setMinimum(1);
    txtNonlinearSteps->setMaximum(100);
    txtNonlinearTolerance = new LineEditDouble(1);

    chkNewtonAutomaticDamping = new QCheckBox(tr("Automatic damping"));
    connect(chkNewtonAutomaticDamping, SIGNAL(stateChanged(int)), this, SLOT(doNewtonDampingChanged(int)));
    lblNewtonDampingCoeff = new QLabel(tr("Damping factor:"));
    txtNewtonDampingCoeff = new LineEditDouble(1);
    lblNewtonDampingNumberToIncrease = new QLabel(tr("Steps to increase DF:"));
    txtNewtonDampingNumberToIncrease = new QSpinBox(this);
    txtNewtonDampingNumberToIncrease->setMinimum(1);
    txtNewtonDampingNumberToIncrease->setMaximum(5);

    chkPicardAndersonAcceleration = new QCheckBox(tr("Use Anderson acceleration"));
    connect(chkPicardAndersonAcceleration, SIGNAL(stateChanged(int)), this, SLOT(doPicardAndersonChanged(int)));
    lblPicardAndersonBeta = new QLabel(tr("Anderson beta:"));
    txtPicardAndersonBeta = new LineEditDouble(0.2);
    lblPicardAndersonNumberOfLastVectors = new QLabel(tr("Num. of last vectors:"));
    txtPicardAndersonNumberOfLastVectors = new QSpinBox(this);
    txtPicardAndersonNumberOfLastVectors->setMinimum(1);
    txtPicardAndersonNumberOfLastVectors->setMaximum(5);

    connect(cmbAdaptivityType, SIGNAL(currentIndexChanged(int)), this, SLOT(doAdaptivityChanged(int)));
    connect(cmbAnalysisType, SIGNAL(currentIndexChanged(int)), this, SLOT(doAnalysisTypeChanged(int)));
    //connect(cmbAnalysisType, SIGNAL(currentIndexChanged(int)), m_problemDialog, SLOT(doFindCouplings()));

    connect(cmbLinearityType, SIGNAL(currentIndexChanged(int)), this, SLOT(doLinearityTypeChanged(int)));

    // fill combobox
    fillComboBox();

    // table
    QGridLayout *layoutGeneral = new QGridLayout();
    layoutGeneral->setColumnMinimumWidth(0, minWidth);
    layoutGeneral->setColumnStretch(1, 1);
    layoutGeneral->addWidget(new QLabel(tr("Type of analysis:")), 0, 0);
    layoutGeneral->addWidget(cmbAnalysisType, 0, 1);

    QGroupBox *grpGeneral = new QGroupBox(tr("General"));
    grpGeneral->setLayout(layoutGeneral);

    // transient analysis
    QGridLayout *layoutTransientAnalysis = new QGridLayout();
    layoutTransientAnalysis->setColumnMinimumWidth(0, minWidth);
    layoutTransientAnalysis->setColumnStretch(1, 1);
    layoutTransientAnalysis->addWidget(new QLabel(tr("Initial condition:")), 0, 0);
    layoutTransientAnalysis->addWidget(txtTransientInitialCondition, 0, 1);
    layoutTransientAnalysis->addWidget(new QLabel(tr("Time skip (s):")), 1, 0);
    layoutTransientAnalysis->addWidget(txtTransientTimeSkip, 1, 1);

    QGroupBox *grpTransientAnalysis = new QGroupBox(tr("Transient analysis"));
    grpTransientAnalysis->setLayout(layoutTransientAnalysis);

    // harmonic analysis
    QGridLayout *layoutMesh = new QGridLayout();
    layoutMesh->setColumnMinimumWidth(0, minWidth);
    layoutMesh->setColumnStretch(1, 1);
    layoutMesh->addWidget(new QLabel(tr("Number of refinements:")), 0, 0);
    layoutMesh->addWidget(txtNumberOfRefinements, 0, 1);
    layoutMesh->addWidget(new QLabel(tr("Polynomial order:")), 1, 0);
    layoutMesh->addWidget(txtPolynomialOrder, 1, 1);

    QGroupBox *grpMesh = new QGroupBox(tr("Mesh parameters"));
    grpMesh->setLayout(layoutMesh);

    // adaptivity
    QGridLayout *layoutAdaptivity = new QGridLayout();
    layoutAdaptivity->setColumnMinimumWidth(0, minWidth);
    layoutAdaptivity->setColumnStretch(1, 1);
    layoutAdaptivity->addWidget(new QLabel(tr("Type:")), 0, 0);
    layoutAdaptivity->addWidget(cmbAdaptivityType, 0, 1);
    layoutAdaptivity->addWidget(new QLabel(tr("Steps:")), 1, 0);
    layoutAdaptivity->addWidget(txtAdaptivitySteps, 1, 1);
    layoutAdaptivity->addWidget(new QLabel(tr("Tolerance (%):")), 2, 0);
    layoutAdaptivity->addWidget(txtAdaptivityTolerance, 2, 1);
    // todo: meaningfull labels
    layoutAdaptivity->addWidget(new QLabel(tr("Steps back in trans:")), 3, 0);
    layoutAdaptivity->addWidget(txtAdaptivityBackSteps, 3, 1);
    layoutAdaptivity->addWidget(new QLabel(tr("Redone each trans st:")), 4, 0);
    layoutAdaptivity->addWidget(txtAdaptivityRedoneEach, 4, 1);

    QGroupBox *grpAdaptivity = new QGroupBox(tr("Adaptivity"));
    grpAdaptivity->setLayout(layoutAdaptivity);

    // linearity
    QGridLayout *layoutLinearity = new QGridLayout();
    layoutLinearity->setColumnMinimumWidth(0, minWidth);
    layoutLinearity->setColumnStretch(1, 1);
    layoutLinearity->addWidget(new QLabel(tr("Linearity:")), 0, 0);
    layoutLinearity->addWidget(cmbLinearityType, 0, 1);
    layoutLinearity->addWidget(new QLabel(tr("Tolerance (%):")), 1, 0);
    layoutLinearity->addWidget(txtNonlinearTolerance, 1, 1);
    layoutLinearity->addWidget(new QLabel(tr("Steps:")), 2, 0);
    layoutLinearity->addWidget(txtNonlinearSteps, 2, 1);
    layoutLinearity->addWidget(lblNewtonDampingCoeff, 3, 0);
    layoutLinearity->addWidget(txtNewtonDampingCoeff, 3, 1);
    layoutLinearity->addWidget(chkNewtonAutomaticDamping, 4, 0, 1, 2);
    layoutLinearity->addWidget(lblNewtonDampingNumberToIncrease, 5, 0);
    layoutLinearity->addWidget(txtNewtonDampingNumberToIncrease, 5, 1);
    layoutLinearity->addWidget(chkPicardAndersonAcceleration, 6, 0, 1, 2);
    layoutLinearity->addWidget(lblPicardAndersonBeta, 7, 0);
    layoutLinearity->addWidget(txtPicardAndersonBeta, 7, 1);
    layoutLinearity->addWidget(lblPicardAndersonNumberOfLastVectors, 8, 0);
    layoutLinearity->addWidget(txtPicardAndersonNumberOfLastVectors, 8, 1);

    QGroupBox *grpLinearity = new QGroupBox(tr("Solver"));
    grpLinearity->setLayout(layoutLinearity);

    // left
    QVBoxLayout *layoutLeft = new QVBoxLayout();
    layoutLeft->addWidget(grpGeneral);
    layoutLeft->addWidget(grpTransientAnalysis);
    layoutLeft->addWidget(grpAdaptivity);
    layoutLeft->addStretch();

    // right
    QVBoxLayout *layoutRight = new QVBoxLayout();
    layoutRight->addWidget(grpMesh);
    layoutRight->addWidget(grpLinearity);
    layoutRight->addStretch();

    // both
    QHBoxLayout *layoutPanel = new QHBoxLayout();
    layoutPanel->addLayout(layoutLeft);
    layoutPanel->addLayout(layoutRight);

    // equation
    QVBoxLayout *layoutEquation = new QVBoxLayout();
    layoutEquation->addWidget(equationLaTeX);
    layoutEquation->addStretch();

    QGroupBox *grpEquation = new QGroupBox(tr("Partial differential equation"));
    grpEquation->setLayout(layoutEquation);

    QVBoxLayout *layoutProblem = new QVBoxLayout();
    layoutProblem->addWidget(grpEquation);
    layoutProblem->addLayout(layoutPanel);

    setLayout(layoutProblem);

    setMinimumSize(sizeHint());
}

void FieldWidget::fillComboBox()
{
    cmbAdaptivityType->clear();
    cmbAdaptivityType->addItem(adaptivityTypeString(AdaptivityType_None), AdaptivityType_None);
    cmbAdaptivityType->addItem(adaptivityTypeString(AdaptivityType_H), AdaptivityType_H);
    cmbAdaptivityType->addItem(adaptivityTypeString(AdaptivityType_P), AdaptivityType_P);
    cmbAdaptivityType->addItem(adaptivityTypeString(AdaptivityType_HP), AdaptivityType_HP);

    cmbLinearityType->addItem(linearityTypeString(LinearityType_Linear), LinearityType_Linear);
    // if (hermesField->hasNonlinearity())
    {
        cmbLinearityType->addItem(linearityTypeString(LinearityType_Picard), LinearityType_Picard);
        cmbLinearityType->addItem(linearityTypeString(LinearityType_Newton), LinearityType_Newton);
    }

    QMapIterator<AnalysisType, QString> it(availableAnalyses(m_fieldInfo->fieldId()));
    while (it.hasNext())
    {
        it.next();
        cmbAnalysisType->addItem(it.value(), it.key());
    }
}

void FieldWidget::load()
{
    // analysis type
    cmbAnalysisType->setCurrentIndex(cmbAnalysisType->findData(m_fieldInfo->analysisType()));
    if (cmbAnalysisType->currentIndex() == -1)
        cmbAnalysisType->setCurrentIndex(0);
    // adaptivity
    cmbAdaptivityType->setCurrentIndex(cmbAdaptivityType->findData(m_fieldInfo->adaptivityType()));
    txtAdaptivitySteps->setValue(m_fieldInfo->adaptivitySteps());
    txtAdaptivityTolerance->setValue(m_fieldInfo->adaptivityTolerance());
    txtAdaptivityBackSteps->setValue(m_fieldInfo->adaptivityBackSteps());
    txtAdaptivityRedoneEach->setValue(m_fieldInfo->adaptivityRedoneEach());
    //mesh
    txtNumberOfRefinements->setValue(m_fieldInfo->numberOfRefinements());
    txtPolynomialOrder->setValue(m_fieldInfo->polynomialOrder());
    // transient
    txtTransientInitialCondition->setValue(m_fieldInfo->initialCondition());
    txtTransientTimeSkip->setValue(m_fieldInfo->timeSkip());
    // linearity
    cmbLinearityType->setCurrentIndex(cmbLinearityType->findData(m_fieldInfo->linearityType()));
    txtNonlinearSteps->setValue(m_fieldInfo->nonlinearSteps());
    txtNonlinearTolerance->setValue(m_fieldInfo->nonlinearTolerance());
    chkNewtonAutomaticDamping->setChecked(m_fieldInfo->newtonAutomaticDamping());
    txtNewtonDampingCoeff->setValue(m_fieldInfo->newtonDampingCoeff());
    txtNewtonDampingCoeff->setEnabled(!m_fieldInfo->newtonAutomaticDamping());
    txtNewtonDampingNumberToIncrease->setValue(m_fieldInfo->newtonDampingNumberToIncrease());
    txtNewtonDampingNumberToIncrease->setEnabled(m_fieldInfo->newtonAutomaticDamping());
    chkPicardAndersonAcceleration->setChecked(m_fieldInfo->picardAndersonAcceleration());
    txtPicardAndersonBeta->setValue(m_fieldInfo->picardAndersonBeta());
    txtPicardAndersonNumberOfLastVectors->setValue(m_fieldInfo->picardAndersonNumberOfLastVectors());

    doAnalysisTypeChanged(cmbAnalysisType->currentIndex());
}

bool FieldWidget::save()
{
    m_fieldInfo->setAnalysisType((AnalysisType) cmbAnalysisType->itemData(cmbAnalysisType->currentIndex()).toInt());

    // adaptivity
    m_fieldInfo->setAdaptivityType((AdaptivityType) cmbAdaptivityType->itemData(cmbAdaptivityType->currentIndex()).toInt());
    m_fieldInfo->setAdaptivitySteps(txtAdaptivitySteps->value());
    m_fieldInfo->setAdaptivityTolerance(txtAdaptivityTolerance->value());
    m_fieldInfo->setAdaptivityBackSteps(txtAdaptivityBackSteps->value());
    m_fieldInfo->setAdaptivityRedoneEach(txtAdaptivityRedoneEach->value());
    //mesh
    m_fieldInfo->setNumberOfRefinements(txtNumberOfRefinements->value());
    m_fieldInfo->setPolynomialOrder(txtPolynomialOrder->value());
    // transient
    m_fieldInfo->setInitialCondition(txtTransientInitialCondition->value());
    m_fieldInfo->setTimeSkip(txtTransientTimeSkip->value());
    // linearity
    m_fieldInfo->setLinearityType((LinearityType) cmbLinearityType->itemData(cmbLinearityType->currentIndex()).toInt());
    m_fieldInfo->setNonlinearSteps(txtNonlinearSteps->value());
    m_fieldInfo->setNonlinearTolerance(txtNonlinearTolerance->value());
    m_fieldInfo->setNewtonAutomaticDamping(chkNewtonAutomaticDamping->isChecked());
    m_fieldInfo->setNewtonDampingCoeff(txtNewtonDampingCoeff->value());
    m_fieldInfo->setNewtonDampingNumberToIncrease(txtNewtonDampingNumberToIncrease->value());
    m_fieldInfo->setPicardAndersonAcceleration(chkPicardAndersonAcceleration->isChecked());
    m_fieldInfo->setPicardAndersonBeta(txtPicardAndersonBeta->value());
    m_fieldInfo->setPicardAndersonNumberOfLastVectors(txtPicardAndersonNumberOfLastVectors->value());

    return true;
}

void FieldWidget::refresh()
{
    doAnalysisTypeChanged(cmbAnalysisType->currentIndex());
}

FieldInfo *FieldWidget::fieldInfo()
{
    return m_fieldInfo;
}

void FieldWidget::doAnalysisTypeChanged(int index)
{
    // initial condition
    txtTransientInitialCondition->setEnabled((AnalysisType) cmbAnalysisType->itemData(index).toInt() == AnalysisType_Transient);

    // time steps skip
    bool otherFieldIsTransient = false;
    foreach (FieldInfo* otherFieldInfo, Agros2D::problem()->fieldInfos())
        if (otherFieldInfo->analysisType() == AnalysisType_Transient && otherFieldInfo->fieldId() != m_fieldInfo->fieldId())
            otherFieldIsTransient = true;

    txtTransientTimeSkip->setEnabled((AnalysisType) cmbAnalysisType->itemData(index).toInt() != AnalysisType_Transient && otherFieldIsTransient);

    doShowEquation();
}

void FieldWidget::doShowEquation()
{
    Module::ModuleAgros module(m_fieldInfo->fieldId(),
                               Agros2D::problem()->config()->coordinateType(),
                               (AnalysisType) cmbAnalysisType->itemData(cmbAnalysisType->currentIndex()).toInt());

    equationLaTeX->setLatex(module.equation());
}

void FieldWidget::doAdaptivityChanged(int index)
{
    txtAdaptivitySteps->setEnabled((AdaptivityType) cmbAdaptivityType->itemData(index).toInt() != AdaptivityType_None);
    txtAdaptivityTolerance->setEnabled((AdaptivityType) cmbAdaptivityType->itemData(index).toInt() != AdaptivityType_None);
    txtAdaptivityBackSteps->setEnabled(Agros2D::problem()->isTransient() && (AdaptivityType) cmbAdaptivityType->itemData(index).toInt() != AdaptivityType_None);
    txtAdaptivityRedoneEach->setEnabled(Agros2D::problem()->isTransient() && (AdaptivityType) cmbAdaptivityType->itemData(index).toInt() != AdaptivityType_None);
}

void FieldWidget::doLinearityTypeChanged(int index)
{
    txtNonlinearSteps->setEnabled((LinearityType) cmbLinearityType->itemData(index).toInt() != LinearityType_Linear);
    txtNonlinearTolerance->setEnabled((LinearityType) cmbLinearityType->itemData(index).toInt() != LinearityType_Linear);

    chkNewtonAutomaticDamping->setVisible((LinearityType) cmbLinearityType->itemData(index).toInt() == LinearityType_Newton);
    lblNewtonDampingCoeff->setVisible((LinearityType) cmbLinearityType->itemData(index).toInt() == LinearityType_Newton);
    txtNewtonDampingCoeff->setVisible((LinearityType) cmbLinearityType->itemData(index).toInt() == LinearityType_Newton);
    lblNewtonDampingNumberToIncrease->setVisible((LinearityType) cmbLinearityType->itemData(index).toInt() == LinearityType_Newton);
    txtNewtonDampingNumberToIncrease->setVisible((LinearityType) cmbLinearityType->itemData(index).toInt() == LinearityType_Newton);
    doNewtonDampingChanged(-1);

    chkPicardAndersonAcceleration->setVisible((LinearityType) cmbLinearityType->itemData(index).toInt() == LinearityType_Picard);
    lblPicardAndersonBeta->setVisible((LinearityType) cmbLinearityType->itemData(index).toInt() == LinearityType_Picard);
    txtPicardAndersonBeta->setVisible((LinearityType) cmbLinearityType->itemData(index).toInt() == LinearityType_Picard);
    lblPicardAndersonNumberOfLastVectors->setVisible((LinearityType) cmbLinearityType->itemData(index).toInt() == LinearityType_Picard);
    txtPicardAndersonNumberOfLastVectors->setVisible((LinearityType) cmbLinearityType->itemData(index).toInt() == LinearityType_Picard);
    doPicardAndersonChanged(-1);
}

void FieldWidget::doNewtonDampingChanged(int index)
{
    txtNewtonDampingCoeff->setEnabled(!chkNewtonAutomaticDamping->isChecked());
    txtNewtonDampingNumberToIncrease->setEnabled(chkNewtonAutomaticDamping->isChecked());
}

void FieldWidget::doPicardAndersonChanged(int index)
{
    txtPicardAndersonBeta->setEnabled(chkPicardAndersonAcceleration->isChecked());
    txtPicardAndersonNumberOfLastVectors->setEnabled(chkPicardAndersonAcceleration->isChecked());
}

// ********************************************************************************************

FieldDialog::FieldDialog(FieldInfo *fieldInfo, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(fieldInfo->name());

    fieldWidget = new FieldWidget(fieldInfo, this);

    // dialog buttons
    QPushButton *btnDeleteField = new QPushButton(tr("Delete field"));
    btnDeleteField->setDefault(false);
    btnDeleteField->setEnabled(Agros2D::problem()->hasField(fieldInfo->fieldId()));
    connect(btnDeleteField, SIGNAL(clicked()), this, SLOT(deleteField()));

    QPushButton *btnModuleEditor = new QPushButton(tr("Module editor"));
    btnModuleEditor->setDefault(false);
    connect(btnModuleEditor, SIGNAL(clicked()), this, SLOT(moduleEditor()));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->addButton(btnDeleteField, QDialogButtonBox::ActionRole);
    buttonBox->addButton(btnModuleEditor, QDialogButtonBox::ActionRole);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(doAccept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(fieldWidget);
    layout->addStretch();
    layout->addWidget(buttonBox);

    setLayout(layout);

    setMaximumSize(sizeHint());

    QSettings settings;
    restoreGeometry(settings.value("FieldDialog/Geometry", saveGeometry()).toByteArray());
}

FieldDialog::~FieldDialog()
{
    QSettings settings;
    settings.setValue("FieldDialog/Geometry", saveGeometry());
}

void FieldDialog::doAccept()
{
    fieldWidget->save();
    accept();
}

void FieldDialog::deleteField()
{
    if (QMessageBox::question(this, tr("Delete"), tr("Physical field '%1' will be pernamently deleted. Are you sure?").
                              arg(fieldWidget->fieldInfo()->name()), tr("&Yes"), tr("&No")) == 0)
    {
        Agros2D::problem()->removeField(fieldWidget->fieldInfo());
        accept();
    }
}

void FieldDialog::moduleEditor()
{
    ModuleDialog moduleDialog(fieldWidget->fieldInfo()->fieldId(), this);
    moduleDialog.exec();
}

// ********************************************************************************************

FieldsToobar::FieldsToobar(QWidget *parent) : QWidget(parent)
{
    createControls();

    connect(Agros2D::problem(), SIGNAL(fieldsChanged()), this, SLOT(refresh()));
    connect(Agros2D::scene(), SIGNAL(invalidated()), this, SLOT(refresh()));

    connect(currentPythonEngineAgros(), SIGNAL(executedScript()), this, SLOT(refresh()));

    refresh();
}

void FieldsToobar::createControls()
{
    actFieldsGroup = new QActionGroup(this);
    connect(actFieldsGroup, SIGNAL(triggered(QAction *)), this, SLOT(fieldDialog(QAction *)));

    tlbFields = new QToolBar(this);
    tlbFields->setIconSize(QSize(36, 36));
    tlbFields->setOrientation(Qt::Horizontal);
    tlbFields->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    tlbFields->setStyleSheet("QToolButton { font-size: 8pt; }");

    // dialog buttons
    QPushButton *btnAddField = new QPushButton(tr("Add field")); // icon("tabadd")
    connect(btnAddField, SIGNAL(clicked()), SLOT(addField()));

    QHBoxLayout *layoutButtons = new QHBoxLayout();
    layoutButtons->addStretch();
    layoutButtons->addWidget(btnAddField);

    QVBoxLayout *layoutToolBar = new QVBoxLayout();
    layoutToolBar->setContentsMargins(0, 1, 0, 0);
    layoutToolBar->addWidget(tlbFields);
    layoutToolBar->addLayout(layoutButtons);
    layoutToolBar->addStretch();

    setLayout(layoutToolBar);
}

void FieldsToobar::refresh()
{
    // fields
    tlbFields->clear();
    actFieldsGroup->actions().clear();

    foreach (FieldInfo *fieldInfo, Agros2D::problem()->fieldInfos())
    {
        QString hint = tr("<h3>%1</h3>"
                          "<table>").arg(fieldInfo->name());
        hint += tr("<tr><td><b>Analysis:</b></td><td>%1</td></tr>"
                   "<tr><td><b>Number of refinements:</b></td><td>%2</td></tr>"
                   "<tr><td><b>Polynomial order:</b></td><td>%3</td></tr>")
                .arg(analysisTypeString(fieldInfo->analysisType()))
                .arg(fieldInfo->numberOfRefinements())
                .arg(fieldInfo->polynomialOrder());

        // adaptivity
        hint += tr("<tr><td><b>Adaptivity:</b></td><td>%1</td></tr>")
                .arg(adaptivityTypeString(fieldInfo->adaptivityType()));
        if (fieldInfo->adaptivityType() != AdaptivityType_None)
        {
            hint += tr("<tr><td><b>&nbsp;&nbsp;&nbsp;Steps:</b></td><td>%1</td></tr>"
                       "<tr><td><b>&nbsp;&nbsp;&nbsp;Tolerance:</b></td><td>%2</td></tr>")
                    .arg(fieldInfo->adaptivitySteps())
                    .arg(fieldInfo->adaptivityTolerance());
        }

        // linearity
        hint += tr("<tr><td><b>Solver:</b></td><td>%1</td></tr>")
                .arg(linearityTypeString(fieldInfo->linearityType()));
        if (fieldInfo->linearityType() != LinearityType_Linear)
        {
            hint += tr("<tr><td><b>&nbsp;&nbsp;&nbsp;Steps:</b></td><td>%1</td></tr>"
                       "<tr><td><b>&nbsp;&nbsp;&nbsp;Tolerance:</b></td><td>%2</td></tr>")
                    .arg(fieldInfo->nonlinearSteps())
                    .arg(fieldInfo->nonlinearTolerance());
        }

        hint += tr("</table>");

        QAction *actField = new QAction(fieldInfo->module() ? fieldInfo->name() : fieldInfo->fieldId(), this);
        actField->setIcon(icon("fields/" + fieldInfo->fieldId()));
        actField->setData(fieldInfo->fieldId());
        actField->setToolTip(hint);

        actFieldsGroup->addAction(actField);
        tlbFields->addAction(actField);
    }

    // spacing
    QLabel *spacing = new QLabel;
    spacing->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    tlbFields->addWidget(spacing);
}

void FieldsToobar::fieldDialog(QAction *action)
{
    FieldInfo *fieldInfo = Agros2D::problem()->fieldInfo(action->data().toString());
    if (fieldInfo)
    {
        FieldDialog fieldDialog(fieldInfo, this);
        if (fieldDialog.exec() == QDialog::Accepted)
        {
            refresh();
            emit changed();
        }
    }
}

void FieldsToobar::addField()
{
    // select field dialog
    FieldSelectDialog dialog(Agros2D::problem()->fieldInfos().keys(), this);
    if (dialog.showDialog() == QDialog::Accepted)
    {
        // add field
        FieldInfo *fieldInfo = new FieldInfo(dialog.selectedFieldId());

        FieldDialog fieldDialog(fieldInfo, this);
        if (fieldDialog.exec() == QDialog::Accepted)
        {
            Agros2D::problem()->addField(fieldInfo);

            refresh();
            emit changed();
        }
        else
        {
            delete fieldInfo;
        }
    }
}

// ********************************************************************************************

CouplingsWidget::CouplingsWidget(QWidget *parent) : QWidget(parent)
{
    Agros2D::problem()->synchronizeCouplings();

    layoutTable = NULL;
    createContent();

    connect(Agros2D::problem(), SIGNAL(fieldsChanged()), this, SLOT(refresh()));

    load();
}

void CouplingsWidget::createContent()
{
    if (layoutTable)
    {
        delete layoutTable;
        qDeleteAll(this->children());
    }

    layoutTable = new QGridLayout();
    layoutTable->setColumnMinimumWidth(0, minWidth);
    layoutTable->setColumnStretch(1, 1);

    m_comboBoxes.clear();
    int line = 0;
    foreach (CouplingInfo *couplingInfo, Agros2D::problem()->couplingInfos())
    {
        layoutTable->addWidget(new QLabel(couplingInfo->coupling()->name()), line, 0);

        m_comboBoxes[couplingInfo] = new QComboBox();

        layoutTable->addWidget(m_comboBoxes[couplingInfo], line, 1);

        line++;
    }

    fillComboBox();
    load();

    foreach (CouplingInfo *couplingInfo, Agros2D::problem()->couplingInfos())
    {
        connect(m_comboBoxes[couplingInfo], SIGNAL(currentIndexChanged(int)), this, SLOT(itemChanged(int)));
    }

    setLayout(layoutTable);
}

void CouplingsWidget::fillComboBox()
{
    foreach (QComboBox* comboBox, m_comboBoxes)
    {
        comboBox->addItem(couplingTypeString(CouplingType_None), CouplingType_None);
        comboBox->addItem(couplingTypeString(CouplingType_Weak), CouplingType_Weak);
        comboBox->addItem(couplingTypeString(CouplingType_Hard), CouplingType_Hard);
    }
}

void CouplingsWidget::load()
{
    foreach (CouplingInfo *couplingInfo, Agros2D::problem()->couplingInfos())
        m_comboBoxes[couplingInfo]->setCurrentIndex(m_comboBoxes[couplingInfo]->findData(couplingInfo->couplingType()));
}

void CouplingsWidget::save()
{
    foreach (CouplingInfo *couplingInfo, Agros2D::problem()->couplingInfos())
        if (m_comboBoxes.contains(couplingInfo))
            couplingInfo->setCouplingType((CouplingType) m_comboBoxes[couplingInfo]->itemData(m_comboBoxes[couplingInfo]->currentIndex()).toInt());

    //Agros2D::problem()->setCouplingInfos(Agros2D::problem()->couplingInfos());
}

void CouplingsWidget::refresh()
{
    Agros2D::problem()->synchronizeCouplings();

    createContent();
}

void CouplingsWidget::itemChanged(int index)
{
    emit changed();
}

// ********************************************************************************************

ProblemWidget::ProblemWidget(QWidget *parent) : QWidget(parent)
{
    createActions();
    createControls();

    updateControls();

    // global signals
    connect(Agros2D::scene(), SIGNAL(invalidated()), this, SLOT(updateControls()));
    connect(Agros2D::problem(), SIGNAL(fieldsChanged()), this, SLOT(updateControls()));
    connect(fieldsToolbar, SIGNAL(changed()), this, SLOT(updateControls()));

    // resend signal
    connect(fieldsToolbar, SIGNAL(changed()), this, SIGNAL(changed()));
    connect(couplingsWidget, SIGNAL(changed()), this, SIGNAL(changed()));

    setMinimumSize(sizeHint());
}

void ProblemWidget::createActions()
{
    actProperties = new QAction(icon("document-properties"), tr("Properties"), this);
    actProperties->setShortcut(tr("F12"));
    actProperties->setStatusTip(tr("Problem properties"));
    actProperties->setCheckable(true);
}

void ProblemWidget::createControls()
{
    // tab
    QTabWidget *tabType = new QTabWidget();
    tabType->addTab(createControlsGeneral(), icon(""), tr("General"));
    tabType->addTab(createControlsScriptAndDescription(), icon(""), tr("Script and description"));

    // fields toolbar
    fieldsToolbar = new FieldsToobar(this);
    QVBoxLayout *layoutFields = new QVBoxLayout();
    layoutFields->addWidget(fieldsToolbar);
    layoutFields->addStretch();

    QGroupBox *grpFieldsToolbar = new QGroupBox(tr("Physical fields"));
    grpFieldsToolbar->setLayout(layoutFields);

    // couplings
    couplingsWidget = new CouplingsWidget(this);
    connect(couplingsWidget, SIGNAL(changed()), couplingsWidget, SLOT(save()));

    QVBoxLayout *layoutCouplings = new QVBoxLayout();
    layoutCouplings->addWidget(couplingsWidget);

    grpCouplings = new QGroupBox(tr("Couplings"));
    grpCouplings->setLayout(layoutCouplings);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(tabType);
    layout->addWidget(grpFieldsToolbar);
    layout->addWidget(grpCouplings);
    layout->addStretch();

    setLayout(layout);
}

QWidget *ProblemWidget::createControlsGeneral()
{
    // problem
    cmbCoordinateType = new QComboBox();
    txtName = new QLineEdit("");

    // matrix solver
    cmbMatrixSolver = new QComboBox();

    // mesh type
    cmbMeshType = new QComboBox();

    // harmonic
    txtFrequency = new LineEditDouble();

    // transient
    cmbTransientMethod = new QComboBox();
    txtTransientOrder = new QSpinBox();
    txtTransientOrder->setMinimum(1);
    txtTransientOrder->setMaximum(3);
    txtTransientTimeTotal = new ValueLineEdit();
    txtTransientTolerance = new ValueLineEdit();
    txtTransientSteps = new QSpinBox();
    txtTransientSteps->setMinimum(1);
    txtTransientSteps->setMaximum(10000);
    lblTransientTimeStep = new QLabel("0.0");
    lblTransientSteps = new QLabel(tr("Number of constant steps:"));

    // fill combobox
    fillComboBox();

    // general
    QGridLayout *layoutGeneral = new QGridLayout();
    layoutGeneral->setColumnMinimumWidth(0, minWidth);
    layoutGeneral->setColumnStretch(1, 1);
    layoutGeneral->addWidget(new QLabel(tr("Coordinate type:")), 0, 0);
    layoutGeneral->addWidget(cmbCoordinateType, 0, 1);
    layoutGeneral->addWidget(new QLabel(tr("Linear solver:")), 1, 0);
    layoutGeneral->addWidget(cmbMatrixSolver, 1, 1);
    layoutGeneral->addWidget(new QLabel(tr("Mesh type:")), 2, 0);
    layoutGeneral->addWidget(cmbMeshType, 2, 1);

    QGroupBox *grpGeneral = new QGroupBox(tr("General"));
    grpGeneral->setLayout(layoutGeneral);

    // harmonic analysis
    QGridLayout *layoutHarmonicAnalysis = new QGridLayout();
    layoutHarmonicAnalysis->setColumnMinimumWidth(0, minWidth);
    layoutHarmonicAnalysis->addWidget(new QLabel(tr("Frequency (Hz):")), 0, 0);
    layoutHarmonicAnalysis->addWidget(txtFrequency, 0, 1);

    grpHarmonicAnalysis = new QGroupBox(tr("Harmonic analysis"));
    grpHarmonicAnalysis->setLayout(layoutHarmonicAnalysis);

    // transient analysis
    QGridLayout *layoutTransientAnalysis = new QGridLayout();
    layoutTransientAnalysis->setColumnMinimumWidth(0, minWidth);
    layoutTransientAnalysis->setColumnStretch(1, 1);
    layoutTransientAnalysis->addWidget(new QLabel(tr("Method:")), 0, 0);
    layoutTransientAnalysis->addWidget(cmbTransientMethod, 0, 1);
    layoutTransientAnalysis->addWidget(new QLabel(tr("Order:")), 1, 0);
    layoutTransientAnalysis->addWidget(txtTransientOrder, 1, 1);
    layoutTransientAnalysis->addWidget(new QLabel(tr("Tolerance:")), 2, 0);
    layoutTransientAnalysis->addWidget(txtTransientTolerance, 2, 1);
    layoutTransientAnalysis->addWidget(new QLabel(tr("Total time (s):")), 3, 0);
    layoutTransientAnalysis->addWidget(txtTransientTimeTotal, 3, 1);
    layoutTransientAnalysis->addWidget(lblTransientSteps, 4, 0);
    layoutTransientAnalysis->addWidget(txtTransientSteps, 4, 1);
    layoutTransientAnalysis->addWidget(new QLabel(tr("Constant time step:")), 5, 0);
    layoutTransientAnalysis->addWidget(lblTransientTimeStep, 5, 1);

    grpTransientAnalysis = new QGroupBox(tr("Transient analysis"));
    grpTransientAnalysis->setLayout(layoutTransientAnalysis);

    // both
    QVBoxLayout *layoutPanel = new QVBoxLayout();
    layoutPanel->addWidget(grpGeneral);
    layoutPanel->addWidget(grpHarmonicAnalysis);
    layoutPanel->addWidget(grpTransientAnalysis);
    layoutPanel->addStretch();

    // name
    QGridLayout *layoutName = new QGridLayout();
    layoutName->addWidget(new QLabel(tr("Name:")), 0, 0);
    layoutName->addWidget(txtName, 0, 1);

    QVBoxLayout *layoutProblem = new QVBoxLayout();
    layoutProblem->addLayout(layoutName);
    layoutProblem->addLayout(layoutPanel);
    layoutProblem->addStretch();

    QWidget *widMain = new QWidget();
    widMain->setLayout(layoutProblem);

    return widMain;
}

QWidget *ProblemWidget::createControlsScriptAndDescription()
{
    // startup script
    txtStartupScript = new ScriptEditor(currentPythonEngine(), this);
    lblStartupScriptError = new QLabel();

    QPalette palette = lblStartupScriptError->palette();
    palette.setColor(QPalette::WindowText, QColor(Qt::red));
    lblStartupScriptError->setPalette(palette);

    QVBoxLayout *layoutStartup = new QVBoxLayout();
    layoutStartup->addWidget(txtStartupScript);
    layoutStartup->addWidget(lblStartupScriptError);

    QGroupBox *grpStartup = new QGroupBox(tr("Startup script"));
    grpStartup->setLayout(layoutStartup);

    // description
    txtDescription = new QTextEdit(this);
    txtDescription->setAcceptRichText(false);

    QVBoxLayout *layoutDescription = new QVBoxLayout();
    layoutDescription->addWidget(txtDescription);

    QGroupBox *grpDescription = new QGroupBox(tr("Description"));
    grpDescription->setLayout(layoutDescription);

    // layout
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(grpStartup, 2);
    layout->addWidget(grpDescription, 1);
    layout->addStretch();

    QWidget *widget = new QWidget();
    widget->setLayout(layout);

    return widget;
}

void ProblemWidget::fillComboBox()
{
    cmbCoordinateType->clear();
    cmbCoordinateType->addItem(coordinateTypeString(CoordinateType_Planar), CoordinateType_Planar);
    cmbCoordinateType->addItem(coordinateTypeString(CoordinateType_Axisymmetric), CoordinateType_Axisymmetric);

    cmbMeshType->addItem(meshTypeString(MeshType_Triangle), MeshType_Triangle);
    cmbMeshType->addItem(meshTypeString(MeshType_Triangle_QuadFineDivision), MeshType_Triangle_QuadFineDivision);
    cmbMeshType->addItem(meshTypeString(MeshType_Triangle_QuadRoughDivision), MeshType_Triangle_QuadRoughDivision);
    cmbMeshType->addItem(meshTypeString(MeshType_Triangle_QuadJoin), MeshType_Triangle_QuadJoin);
    cmbMeshType->addItem(meshTypeString(MeshType_GMSH_Triangle), MeshType_GMSH_Triangle);
    cmbMeshType->addItem(meshTypeString(MeshType_GMSH_Quad), MeshType_GMSH_Quad);
    cmbMeshType->addItem(meshTypeString(MeshType_GMSH_QuadDelaunay_Experimental), MeshType_GMSH_QuadDelaunay_Experimental);

    cmbMatrixSolver->addItem(matrixSolverTypeString(Hermes::SOLVER_UMFPACK), Hermes::SOLVER_UMFPACK);
#ifdef WITH_MUMPS
    cmbMatrixSolver->addItem(matrixSolverTypeString(Hermes::SOLVER_MUMPS), Hermes::SOLVER_MUMPS);
#endif
#ifdef WITH_SUPERLU
    cmbMatrixSolver->addItem(matrixSolverTypeString(Hermes::SOLVER_SUPERLU), Hermes::SOLVER_SUPERLU);
#endif

    cmbTransientMethod->addItem(timeStepMethodString(TimeStepMethod_Fixed), TimeStepMethod_Fixed);
    cmbTransientMethod->addItem(timeStepMethodString(TimeStepMethod_BDFTolerance), TimeStepMethod_BDFTolerance);
    cmbTransientMethod->addItem(timeStepMethodString(TimeStepMethod_BDFNumSteps), TimeStepMethod_BDFNumSteps);
}

void ProblemWidget::updateControls()
{
    // disconnect signals
    // without clearing solution
    txtName->disconnect();
    txtDescription->disconnect();

    // with clearing solution
    cmbCoordinateType->disconnect();
    cmbMatrixSolver->disconnect();
    cmbMeshType->disconnect();

    cmbMeshType->disconnect();
    connect(txtFrequency, SIGNAL(editingFinished()), this, SLOT(changedWithClear()));

    cmbTransientMethod->disconnect();
    txtTransientOrder->disconnect();
    txtTransientTimeTotal->disconnect();
    txtTransientTolerance->disconnect();
    txtTransientSteps->disconnect();

    txtStartupScript->disconnect();

    // main
    txtName->setText(Agros2D::problem()->config()->name());
    cmbCoordinateType->setCurrentIndex(cmbCoordinateType->findData(Agros2D::problem()->config()->coordinateType()));
    if (cmbCoordinateType->currentIndex() == -1)
        cmbCoordinateType->setCurrentIndex(0);

    // mesh type
    cmbMeshType->setCurrentIndex(cmbMeshType->findData(Agros2D::problem()->config()->meshType()));

    // harmonic magnetic
    grpHarmonicAnalysis->setVisible(Agros2D::problem()->isHarmonic());
    txtFrequency->setValue(Agros2D::problem()->config()->frequency());
    // txtFrequency->setEnabled(Agros2D::problem()->isHarmonic());

    // transient
    grpTransientAnalysis->setVisible(Agros2D::problem()->isTransient());
    txtTransientSteps->setValue(Agros2D::problem()->config()->numConstantTimeSteps());
    // txtTransientTimeStep->setEnabled(Agros2D::problem()->isTransient());
    txtTransientTimeTotal->setValue(Agros2D::problem()->config()->timeTotal());
    txtTransientTolerance->setValue(Agros2D::problem()->config()->timeMethodTolerance());
    // txtTransientTimeTotal->setEnabled(Agros2D::problem()->isTransient());
    txtTransientOrder->setValue(Agros2D::problem()->config()->timeOrder());
    cmbTransientMethod->setCurrentIndex(cmbTransientMethod->findData(Agros2D::problem()->config()->timeStepMethod()));
    if (cmbTransientMethod->currentIndex() == -1)
        cmbTransientMethod->setCurrentIndex(0);

    // matrix solver
    cmbMatrixSolver->setCurrentIndex(cmbMatrixSolver->findData(Agros2D::problem()->config()->matrixSolver()));

    // startup
    txtStartupScript->setPlainText(Agros2D::problem()->config()->startupscript());

    // description
    txtDescription->setPlainText(Agros2D::problem()->config()->description());

    // couplings
    fieldsToolbar->refresh();
    couplingsWidget->refresh();

    grpCouplings->setVisible(Agros2D::problem()->couplingInfos().count() > 0);

    transientChanged();

    // connect signals
    // without clearing solution
    connect(txtName, SIGNAL(editingFinished()), this, SLOT(changedWithoutClear()));
    connect(txtDescription, SIGNAL(textChanged()), this, SLOT(changedWithoutClear()));

    // with clearing solution
    connect(cmbCoordinateType, SIGNAL(currentIndexChanged(int)), this, SLOT(changedWithClear()));
    connect(cmbMatrixSolver, SIGNAL(currentIndexChanged(int)), this, SLOT(changedWithClear()));
    connect(cmbMeshType, SIGNAL(currentIndexChanged(int)), this, SLOT(changedWithClear()));

    connect(txtFrequency, SIGNAL(editingFinished()), this, SLOT(changedWithClear()));

    connect(cmbTransientMethod, SIGNAL(currentIndexChanged(int)), this, SLOT(changedWithClear()));
    connect(txtTransientOrder, SIGNAL(editingFinished()), this, SLOT(changedWithClear()));
    connect(txtTransientTimeTotal, SIGNAL(editingFinished()), this, SLOT(changedWithClear()));
    connect(txtTransientTolerance, SIGNAL(editingFinished()), this, SLOT(changedWithClear()));
    connect(txtTransientSteps, SIGNAL(editingFinished()), this, SLOT(changedWithClear()));

    connect(txtStartupScript, SIGNAL(textChanged()), this, SLOT(startupScriptChanged()));
    connect(txtStartupScript, SIGNAL(textChanged()), this, SLOT(changedWithClear()));

    // transient
    connect(txtTransientSteps, SIGNAL(editingFinished()), this, SLOT(transientChanged()));
    connect(txtTransientTimeTotal, SIGNAL(editingFinished()), this, SLOT(transientChanged()));
    connect(txtTransientOrder, SIGNAL(editingFinished()), this, SLOT(transientChanged()));
    connect(cmbTransientMethod, SIGNAL(currentIndexChanged(int)), this, SLOT(transientChanged()));
}

void ProblemWidget::changedWithoutClear()
{
    Agros2D::problem()->config()->setName(txtName->text());
    Agros2D::problem()->config()->setDescription(txtDescription->toPlainText());
}

void ProblemWidget::changedWithClear()
{
    // run and check startup script
    if (!txtStartupScript->toPlainText().isEmpty())
    {
        ScriptResult scriptResult = currentPythonEngineAgros()->runScript(txtStartupScript->toPlainText());
        if (scriptResult.isError)
            return;
    }

    // save properties
    Agros2D::problem()->config()->blockSignals(true);

    Agros2D::problem()->config()->setCoordinateType((CoordinateType) cmbCoordinateType->itemData(cmbCoordinateType->currentIndex()).toInt());
    Agros2D::problem()->config()->setMeshType((MeshType) cmbMeshType->itemData(cmbMeshType->currentIndex()).toInt());

    Agros2D::problem()->config()->setFrequency(txtFrequency->value());

    Agros2D::problem()->config()->setTimeStepMethod((TimeStepMethod) cmbTransientMethod->itemData(cmbTransientMethod->currentIndex()).toInt());
    Agros2D::problem()->config()->setTimeOrder(txtTransientOrder->value());
    Agros2D::problem()->config()->setTimeMethodTolerance(txtTransientTolerance->value());
    Agros2D::problem()->config()->setNumConstantTimeSteps(txtTransientSteps->value());
    Agros2D::problem()->config()->setTimeTotal(txtTransientTimeTotal->value());

    // matrix solver
    Agros2D::problem()->config()->setMatrixSolver((Hermes::MatrixSolverType) cmbMatrixSolver->itemData(cmbMatrixSolver->currentIndex()).toInt());

    // script
    Agros2D::problem()->config()->setStartupScript(txtStartupScript->toPlainText());

    // save couplings
    couplingsWidget->save();

    Agros2D::problem()->config()->blockSignals(false);
    Agros2D::problem()->config()->refresh();

    emit changed();
}

void ProblemWidget::startupScriptChanged()
{
    lblStartupScriptError->clear();

    // run and check startup script
    if (!txtStartupScript->toPlainText().isEmpty())
    {
        ScriptResult scriptResult = currentPythonEngineAgros()->runScript(txtStartupScript->toPlainText());
        if (scriptResult.isError)
            lblStartupScriptError->setText(QObject::tr("Error: %1").arg(scriptResult.text));
    }
}

void ProblemWidget::transientChanged()
{
    if (txtTransientTimeTotal->evaluate(true))
    {
        lblTransientTimeStep->setText(QString("%1 s").arg(txtTransientTimeTotal->number() / txtTransientSteps->value()));
    }

    if(Agros2D::problem()->config()->timeStepMethod() == TimeStepMethod_BDFTolerance)
    {
        txtTransientTolerance->setEnabled(true);
        txtTransientSteps->setEnabled(false);
    }
    else
    {
        txtTransientTolerance->setEnabled(false);
        txtTransientSteps->setEnabled(true);
        if(Agros2D::problem()->config()->timeStepMethod() == TimeStepMethod_Fixed)
            lblTransientSteps->setText(tr("Number of steps:"));
        else
            lblTransientSteps->setText(tr("Aprox. number of steps:"));
    }


}

