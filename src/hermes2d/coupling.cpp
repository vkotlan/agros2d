#include "coupling.h"
#include "scene.h"
#include "util.h"
#include "module.h"
#include "weakform_parser.h"
#include "hermes2d/field.h"
#include "hermes2d/problem.h"

#include <dirent.h>

#include "../../resources_source/classes/coupling_xml.h"

CouplingInfo::CouplingInfo(FieldInfo *sourceField, FieldInfo *targetField) :
    m_sourceField(sourceField), m_targetField(targetField)
{
    //TODO in each module should be implicit value
    m_couplingType = CouplingType_Weak;
    m_coupling = NULL;

    reload();
}

CouplingInfo::~CouplingInfo()
{
    if (m_coupling)
        delete m_coupling;
}

void CouplingInfo::setCouplingType(CouplingType couplingType)
{
    m_couplingType = couplingType;
    reload();
}

void CouplingInfo::reload()
{
    if (m_coupling)
        delete m_coupling;

    m_coupling = couplingFactory(m_sourceField, m_targetField, m_couplingType);
}

LinearityType CouplingInfo::linearityType()
{
    assert(sourceField()->linearityType() == targetField()->linearityType());
    return sourceField()->linearityType();
}

bool isCouplingAvailable(FieldInfo* sourceField, FieldInfo* targetField)
{
    QDir dir(datadir() + COUPLINGROOT);
    if (!dir.exists())
        error("Couplings dir '%s' doesn't exists", (datadir() + COUPLINGROOT).toStdString().c_str());

    QStringList filter;
    filter << "*.xml";
    QStringList list = dir.entryList(filter);

    foreach (QString filename, list)
    {
        // read name
        std::auto_ptr<XMLCoupling::coupling> couplings_xsd = XMLCoupling::coupling_((datadir() + COUPLINGROOT + "/" + filename).toStdString().c_str());
        XMLCoupling::coupling *coup = couplings_xsd.get();

        // module name
        QString sourceFieldStr(QString::fromStdString(coup->general().modules().source().id()));
        QString targetFieldStr(QString::fromStdString(coup->general().modules().target().id()));

        if ((sourceFieldStr == sourceField->fieldId()) && (targetFieldStr == targetField->fieldId()))
        {
            // check whether coupling is available for values of source and target fields such as analysis type
            for (int i = 0; i < coup->volume().weakforms_volume().weakform_volume().size(); i++)
            {
                XMLCoupling::weakform_volume wf = coup->volume().weakforms_volume().weakform_volume().at(i);

                if ((wf.sourceanalysis() == analysisTypeToStringKey(sourceField->analysisType()).toStdString()) &&
                        (wf.targetanalysis() == analysisTypeToStringKey(targetField->analysisType()).toStdString()))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

Coupling::Coupling(const QString &couplingId, CoordinateType coordinateType, CouplingType couplingType, AnalysisType sourceFieldAnalysis, AnalysisType targetFieldAnalysis)
{
    m_coordinateType = coordinateType;
    m_couplingType = couplingType;
    m_sourceFieldAnalysis = sourceFieldAnalysis;
    m_targetFieldAnalysis = targetFieldAnalysis;

    clear();

    // read coupling description
    QString filename = (datadir() + COUPLINGROOT + "/" + couplingId + ".xml");
    assert(QFile::exists(filename));

    read(filename);
}

Coupling::~Coupling()
{
    clear();
}

void Coupling::clear()
{
    m_constants.clear();

    m_wfMatrixVolumeExpression.clear();
    m_wfVectorVolumeExpression.clear();
}

mu::Parser* Coupling::expressionParser()
{
    mu::Parser *parser = new mu::Parser();

    // pi
    parser->DefineConst("PI", M_PI);

    // frequency
    parser->DefineConst("f", Util::problem()->config()->frequency());

    // timestep
    parser->DefineConst("dt", Util::problem()->config()->timeStep().number());

    QMapIterator<QString, double> it(m_constants);
    while (it.hasNext())
    {
        it.next();
        parser->DefineConst(it.key().toStdString(), it.value());
    }

    parser->EnableOptimizer(true);

    return parser;
}

void Coupling::read(const QString &filename)
{
    assert(QFile::exists(filename));

    // qDebug() << "reading coupling: " << filename;

    clear();

    // save current locale
    char *plocale = setlocale (LC_NUMERIC, "");
    setlocale (LC_NUMERIC, "C");

    std::auto_ptr<XMLCoupling::coupling> couplings_xsd = XMLCoupling::coupling_(filename.toStdString().c_str());
    XMLCoupling::coupling *coup = couplings_xsd.get();

    // problem
    m_couplingId = QString::fromStdString(coup->general().id());
    m_name = QString::fromStdString(coup->general().name());
    m_description = QString::fromStdString(coup->general().description());

    // constants
    for (int i = 0; i < coup->constants().constant().size(); i++)
    {
        XMLCoupling::constant cnst = coup->constants().constant().at(i);
        m_constants[QString::fromStdString(cnst.id())] = cnst.value();
    }

    // volume weakforms
    for (int i = 0; i < coup->volume().weakforms_volume().weakform_volume().size(); i++)
    {
        XMLCoupling::weakform_volume wf = coup->volume().weakforms_volume().weakform_volume().at(i);

        if ((wf.couplingtype() == couplingTypeToStringKey(m_couplingType).toStdString()) &&
                (wf.sourceanalysis() == analysisTypeToStringKey(m_sourceFieldAnalysis).toStdString()) &&
                (wf.targetanalysis() == analysisTypeToStringKey(m_targetFieldAnalysis).toStdString()))
        {
            // matrix form
            for (int i = 0; i < wf.matrix_form().size(); i++)
            {
                XMLCoupling::matrix_form form = wf.matrix_form().at(i);
                m_wfMatrixVolumeExpression.push_back(new ParserFormExpression(form.i(), form.j(),
                                                                          (m_coordinateType == CoordinateType_Planar) ? form.planar() : form.axi()));
            }

            // vector form
            for (int i = 0; i < wf.vector_form().size(); i++)
            {
                XMLCoupling::vector_form form = wf.vector_form().at(i);
                m_wfVectorVolumeExpression.push_back(new ParserFormExpression(form.i(), form.j(),
                                                                          (m_coordinateType == CoordinateType_Planar) ? form.planar() : form.axi()));
            }
        }
    }

    // set system locale
    setlocale(LC_NUMERIC, plocale);
}



// ****************************************************************************************************

Coupling *couplingFactory(FieldInfo* sourceField, FieldInfo* targetField, CouplingType couplingType)
{
    // open coupling
    QString couplingId = sourceField->fieldId() + "-" + targetField->fieldId();
    QString filename = (datadir() + COUPLINGROOT + "/" + couplingId + ".xml");

    if (QFile::exists(filename))
    {
        CoordinateType coordinateType = Util::problem()->config()->coordinateType();
        Coupling *coupling = new Coupling(couplingId,
                                          coordinateType,
                                          couplingType,
                                          sourceField->analysisType(),
                                          targetField->analysisType());

        return coupling;
    }
    else
    {
        qDebug() << "Coupling doesn't exists.";
        return NULL;
    }


}

void CouplingInfo::synchronizeCouplings(const QMap<QString, FieldInfo *>& fieldInfos, QMap<QPair<FieldInfo*, FieldInfo* >, CouplingInfo* >& couplingInfos)
{
    // add missing
    foreach (FieldInfo* sourceField, fieldInfos)
    {
        foreach (FieldInfo* targetField, fieldInfos)
        {
            if(sourceField == targetField)
                continue;
            QPair<FieldInfo*, FieldInfo*> fieldInfosPair(sourceField, targetField);
            if (isCouplingAvailable(sourceField, targetField)){
                if (!couplingInfos.contains(fieldInfosPair))
                {
                    couplingInfos[fieldInfosPair] = new CouplingInfo(sourceField, targetField);
                }
            }
        }
    }

    // remove extra
    foreach (CouplingInfo* couplingInfo, couplingInfos)
    {
        if(! (fieldInfos.contains(couplingInfo->sourceField()->fieldId()) &&
              fieldInfos.contains(couplingInfo->targetField()->fieldId()) &&
              isCouplingAvailable(couplingInfo->sourceField(), couplingInfo->targetField())))
        {
            couplingInfos.remove(QPair<FieldInfo*, FieldInfo*>(couplingInfo->sourceField(), couplingInfo->targetField()));
        }
    }
}

