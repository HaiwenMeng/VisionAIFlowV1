#include "QtCircleROI.h"
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QtMath>
#include <QDebug>
#include "ytroishowdisp.h"
QtCircleROI::QtCircleROI(QVector<double> &tdata, QString &key)
{
    m_CMvCircle.GetData(tdata);
    m_Key = key;
    m_types = circleROI;
    m_dRadius = m_CMvCircle.radius;
    Center = QPointF(m_CMvCircle.center.x, m_CMvCircle.center.y);  // 两个控制点
    m_ControlList << new ControlItem(this, QPointF(), 0, ControlItem::Move_Control);
    m_ControlList << new ControlItem(this, QPointF(), 1, ControlItem::Size_Control);
    UpDate();
}

QtCircleROI::~QtCircleROI()
{

}


bool QtCircleROI::UpDate(int index)
{
    if(index > 0)
    {
        QPointF Pf = m_ControlList[index]->GetPoint();
        QPointF tmp = Pf - Center;
        m_dRadius = sqrt(tmp.x() * tmp.x() + tmp.y() * tmp.y());
    }
    else
    {
        m_ControlList[0]->SetPoint(Center);
        m_ControlList[1]->SetPoint(Center + QPointF(m_dRadius, 0));
    }

    m_ItemPath.clear();
    m_ItemPath.addEllipse(Center, m_dRadius, m_dRadius);
    m_ItemPath.closeSubpath();
    return true;
}

void QtCircleROI::toGetItemVal()
{
    QPointF tPoint = this->mapToScene(Center);
    CMvCircle tCircle;
    tCircle.center.x = tPoint.x();
    tCircle.center.y = tPoint.y();
    tCircle.radius = m_dRadius;
    if(IsDataChange(tCircle.Data(), m_CMvCircle.Data()))
    {
        m_CMvCircle = tCircle;
        emit m_YtRoiShowDisp->ROIChange(m_CMvCircle.Data(), m_Key, m_types);

    }
}

QVector<double> QtCircleROI::toGetDatavalue()
{
    return m_CMvCircle.Data();

}
