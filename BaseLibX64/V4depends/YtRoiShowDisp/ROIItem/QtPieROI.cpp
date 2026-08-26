#include "QtPieROI.h"
#include <QCursor>
#include "ytroishowdisp.h"

QtPieROI::QtPieROI(QVector<double> &tdata, QString &key)
{
    m_CMvPie.GetData(tdata);
    m_Key = key;
    m_dStAngle = m_CMvPie.stAngle;
    m_dEdAngle = m_CMvPie.edAngle;
    m_dSpanAngle = m_CMvPie.edAngle - m_CMvPie.stAngle;
    m_dRadisMin = m_CMvPie.RadisMin;
    m_dRadisMax = m_CMvPie.RadisMax;
    m_types =pieROI;
    Center = QPointF(m_CMvPie.center.x, m_CMvPie.center.y);

    m_Polygon << Center + QPointF(m_CMvPie.RadisMin, 0)
            << Center + QPointF(m_CMvPie.RadisMin, 0)
            << Center + QPointF(m_CMvPie.RadisMax, 0)
            << Center + QPointF(m_CMvPie.RadisMax, 0);
    m_lastPolygon = m_Polygon;

    // 0是中心点，1、3是旋转控制点，2、4是尺寸控制点，这个顺序不能变动
    //     0
    //
    //   2    1
    //
    // 4         3
    m_ControlList << new ControlItem(this, QPointF(), 0, ControlItem::Move_Control);
    m_ControlList << new ControlItem(this, QPointF(), 1, ControlItem::Rotate_Control);
    m_ControlList << new ControlItem(this, QPointF(), 2, ControlItem::Size_Control);
    m_ControlList << new ControlItem(this, QPointF(), 3, ControlItem::Rotate_Control);
    m_ControlList << new ControlItem(this, QPointF(), 4, ControlItem::Size_Control);

    UpDate();
}

QtPieROI::~QtPieROI()
{

}

bool QtPieROI::UpDate(int index)
{
    if(index > 0) {
        QPointF Pf = m_ControlList[index]->GetPoint();
        switch (index) {
        case 1:
        case 3:
            AfterRotate(Pf, m_Polygon[index - 1], Center, m_Polygon);
            m_dStAngle = 360-CalculateAngle(m_Polygon[index - 1], Center);
            m_dEdAngle = 360-CalculateAngle(m_Polygon[index], Center);
            break;
        case 2:
        case 4:
            CalculateArcCornerPt(Pf, Center, index, m_Polygon);
            if(index == 2) {
                m_dRadisMin = Distance(m_Polygon[index - 1], Center);
                if(m_dRadisMin >= m_dRadisMax) {
                    m_Polygon = m_lastPolygon;
                    m_dRadisMin = Distance(m_Polygon[index - 1], Center);
                    return false;
                }
            }
            else {
                m_dRadisMax = Distance(m_Polygon[index - 1], Center);
                if(m_dRadisMax <= m_dRadisMin) {
                    m_Polygon = m_lastPolygon;
                    m_dRadisMax = Distance(m_Polygon[index - 1], Center);
                    return false;
                }
            }
            m_lastPolygon = m_Polygon;
            m_dEdAngle = 360-CalculateAngle(m_Polygon[index - 1], Center);

            if(m_dEdAngle > m_dStAngle) {
                m_dSpanAngle = m_dEdAngle - m_dStAngle;
            }
            else {
                m_dSpanAngle = m_dEdAngle + 360 - m_dStAngle;
            }
            break;
        default:
            break;
        }
    }
    else {
        RotateMoveTo(-m_dStAngle / 180 * MV_PI, Center, m_Polygon, 0);
        RotateMoveTo(-m_dEdAngle / 180 * MV_PI, Center, m_Polygon, 1);
        RotateMoveTo(-m_dStAngle / 180 * MV_PI, Center, m_Polygon, 2);
        RotateMoveTo(-m_dEdAngle / 180 * MV_PI, Center, m_Polygon, 3);
    }

    m_ControlList[0]->SetPoint(Center);
    m_ControlList[1]->SetPoint(m_Polygon[0]);
    m_ControlList[2]->SetPoint(m_Polygon[1]);
    m_ControlList[3]->SetPoint(m_Polygon[2]);
    m_ControlList[4]->SetPoint(m_Polygon[3]);

    m_ItemPath.clear();
    QRectF rectMin(Center.x() - m_dRadisMin, Center.y() - m_dRadisMin, m_dRadisMin * 2, m_dRadisMin * 2);
    m_ItemPath.arcMoveTo(rectMin, m_dStAngle);
    m_ItemPath.arcTo(rectMin, m_dStAngle, m_dSpanAngle);
    m_ItemPath.lineTo(Center);
    m_ItemPath.lineTo(m_Polygon[0]);

    QRectF rectMax(Center.x() - m_dRadisMax, Center.y() - m_dRadisMax, m_dRadisMax * 2, m_dRadisMax * 2);
    m_ItemPath.arcMoveTo(rectMax, m_dStAngle);
    m_ItemPath.arcTo(rectMax, m_dStAngle, m_dSpanAngle);
    m_ItemPath.lineTo(Center);
    m_ItemPath.lineTo(m_Polygon[2]);
    m_ItemPath.closeSubpath();

    return true;
}

void QtPieROI::toGetItemVal()
{
    QPointF ptCenter = this->mapToScene(Center);
    CMvPie tMV_PIe;
    tMV_PIe.center.x = ptCenter.x();
    tMV_PIe.center.y = ptCenter.y();
    tMV_PIe.RadisMin = m_dRadisMin;
    tMV_PIe.RadisMax = m_dRadisMax;
    tMV_PIe.stAngle = m_dStAngle;
    tMV_PIe.edAngle = m_dEdAngle;
    if(IsDataChange(tMV_PIe.Data(), m_CMvPie.Data())) {
        m_CMvPie = tMV_PIe;
        emit m_YtRoiShowDisp->ROIChange(m_CMvPie.Data(), m_Key, m_types);
    }
}

QVector<double> QtPieROI::toGetDatavalue()
{
    return m_CMvPie.Data();
}
