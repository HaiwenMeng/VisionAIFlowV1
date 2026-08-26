#include "QtArcROI.h"
#include <QCursor>
#include "ytroishowdisp.h"
QtArcROI::QtArcROI(QVector<double> &tdata, QString &key)
{
    m_CMvArc.GetData(tdata);
    m_Key = key;
    m_dSpanAngle = m_CMvArc.edAngle - m_CMvArc.stAngle;
    m_dStAngle = m_CMvArc.stAngle;
    m_dEdAngle = m_CMvArc.edAngle;
    m_dRadius = m_CMvArc.radius;
    m_types = arcROI;
    Center = QPointF(m_CMvArc.center.x, m_CMvArc.center.y);

    m_polygon << Center + QPointF(m_dRadius, 0)
            << Center + QPointF(m_dRadius, 0);

    // 0是中心点，1是旋转控制点，2是尺寸控制点，这个顺序不能变动
    //     0
    //
    // 2        1
    m_ControlList << new ControlItem(this, QPointF(), 0, ControlItem::Move_Control);
    m_ControlList << new ControlItem(this, QPointF(), 1, ControlItem::Rotate_Control);
    m_ControlList << new ControlItem(this, QPointF(), 2, ControlItem::Size_Control);

    UpDate();
}

QtArcROI::~QtArcROI()
{

}
bool QtArcROI::UpDate(int index)
{
    if(index > 0) {
        QPointF Pf = m_ControlList[index]->GetPoint();
        QPointF tmp = Pf - Center;
        switch (index)
        {
        case 1:
            AfterRotate(Pf, m_polygon[0], Center, m_polygon);
            m_dStAngle = 360-CalculateAngle(m_polygon[0], Center);
            m_dEdAngle = 360-CalculateAngle(m_polygon[1], Center);
            break;
        case 2:
            CalculateArcCornerPt(Pf, Center, index, m_polygon);
            m_dEdAngle = 360-CalculateAngle(m_polygon[1], Center);
            m_dRadius = sqrt(tmp.x() * tmp.x() + tmp.y() * tmp.y());

            if(m_dEdAngle > m_dStAngle) {
                m_dSpanAngle = m_dEdAngle - m_dStAngle;
            }
            else {
                m_dSpanAngle = m_dEdAngle + 360 - m_dStAngle;
            }
            break;
        }
    }
    else {

        RotateMoveTo(-m_dStAngle / 180 * MV_PI, Center, m_polygon, 0);
        RotateMoveTo(-m_dEdAngle / 180 * MV_PI, Center, m_polygon, 1);
    }

    m_ControlList[0]->SetPoint(Center);
    m_ControlList[1]->SetPoint(m_polygon[0]);
    m_ControlList[2]->SetPoint(m_polygon[1]);

    m_ItemPath.clear();
    QRectF rect(Center.x() - m_dRadius, Center.y() - m_dRadius, m_dRadius * 2, m_dRadius * 2);
    m_ItemPath.arcMoveTo(rect, m_dStAngle);
    m_ItemPath.arcTo(rect, m_dStAngle, m_dSpanAngle);
    m_ItemPath.lineTo(Center);
    m_ItemPath.lineTo(m_polygon[0]);
    m_ItemPath.closeSubpath();

    return true;
}

void QtArcROI::toGetItemVal()
{
    QPointF ptCenter = this->mapToScene(Center);
    CMvArc tArc;
    tArc.center.x = ptCenter.x();
    tArc.center.y = ptCenter.y();
    tArc.radius = m_dRadius;
    tArc.stAngle = m_dStAngle;
    tArc.edAngle = m_dEdAngle;
    if(IsDataChange(tArc.Data(), m_CMvArc.Data())) {
        m_CMvArc = tArc;
        emit m_YtRoiShowDisp->ROIChange(m_CMvArc.Data(), m_Key, m_types);
    }
}

QVector<double> QtArcROI::toGetDatavalue()
{
    return m_CMvArc.Data();
}
