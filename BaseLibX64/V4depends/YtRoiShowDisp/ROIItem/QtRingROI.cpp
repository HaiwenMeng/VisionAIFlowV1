#include "QtRingROI.h"
#include "ytroishowdisp.h"

QtRingROI::QtRingROI(QVector<double> &tdata, QString &key)
{
    m_CMvRingCircle.GetData(tdata);
    m_Key = key;
    m_types = ringROI;
    m_Center = QPointF(m_CMvRingCircle.Center.x, m_CMvRingCircle.Center.y);  //两个控制点
    m_ControlList << new ControlItem(this, QPointF(), 0, ControlItem::Move_Control);
    m_ControlList << new ControlItem(this, QPointF(), 1, ControlItem::Size_Control);
    m_ControlList << new ControlItem(this, QPointF(), 2, ControlItem::Size_Control);
    RadiusMin = m_CMvRingCircle.RadisMin;
    RadiusMax = m_CMvRingCircle.RadisMax;
    UpDate();
}
QtRingROI::~QtRingROI()
{

}
bool QtRingROI::UpDate(int index)
{
    if(index > 0)
    {
        QPointF Pf = m_ControlList[index]->GetPoint();
        QPointF tmp = Pf - m_Center;
        qreal R = sqrt(tmp.x() * tmp.x() + tmp.y() * tmp.y());
        if(index == 1)
        {
            if(R > RadiusMax)
                return false;
            RadiusMin = R;
        }
        else if(index == 2)
        {
            if(R < RadiusMin)
                return false;
            RadiusMax = R;
        }
    }
    else
    {
        m_ControlList[0]->SetPoint(m_Center);
        m_ControlList[1]->SetPoint(m_Center + QPointF(RadiusMin,0));
        m_ControlList[2]->SetPoint(m_Center + QPointF(RadiusMax,0));
    }

    m_ItemPath.clear();
    m_ItemPath.addEllipse(m_Center,RadiusMax, RadiusMax);
    m_ItemPath.addEllipse(m_Center,RadiusMin, RadiusMin);
    return true;

}

void QtRingROI::toGetItemVal()
{
    QPointF tPoint = this->mapToScene(m_Center);
    CMvRingCircle tCMvRingCircle;
    tCMvRingCircle.Center.x = tPoint.x();
    tCMvRingCircle.Center.y = tPoint.y();
    tCMvRingCircle.RadisMin = RadiusMin;
    tCMvRingCircle.RadisMax = RadiusMax;
    if(IsDataChange(tCMvRingCircle.Data(), m_CMvRingCircle.Data()))
    {
        m_CMvRingCircle = tCMvRingCircle;
        emit m_YtRoiShowDisp->ROIChange(m_CMvRingCircle.Data(), m_Key, m_types);
    }
}

QVector<double> QtRingROI::toGetDatavalue()
{
    return m_CMvRingCircle.Data();
}
