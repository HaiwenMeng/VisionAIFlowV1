#include "QtPointROI.h"
#include <QDebug>
#include "ytroishowdisp.h"

QtPointROI::QtPointROI(QVector<double> &tdata, QString &key)
{
    m_CMvPoint.GetData(tdata);
    m_Center=QPointF(m_CMvPoint.x,m_CMvPoint.y);
    m_nControlItemSize=m_nControlItemSize/2;
    qDebug()<<"QtPointROI"<<this->scale();
    m_dScale=this->scale();
    m_Key = key;
    m_types =pointROI;
    m_ControlList << new ControlItem(this, QPointF(), 0, ControlItem::Move_Control);
    UpDate();
}

QtPointROI::~QtPointROI()
{

}

bool QtPointROI::UpDate(int index)
{
    qDebug()<<m_dScale<<"QtPointROI::UpDate";

    if(m_dScale != m_ControlList[0]->scale())
    {
        m_dScale = m_ControlList[0]->scale();
    }
    Q_UNUSED(index);
    m_ControlList[0]->SetPoint(m_Center);
    m_ItemPath.clear();
    QPolygonF polygon[2];
    double dLineLen = m_nControlItemSize*8*m_dScale;

    polygon[0].append(m_Center + QPointF(dLineLen, 0));
    polygon[0].append(m_Center + QPointF(-dLineLen, 0));
    polygon[1].append(m_Center + QPointF(0, dLineLen));
    polygon[1].append(m_Center + QPointF(0, -dLineLen));
    m_ItemPath.addPolygon(polygon[0]);
    m_ItemPath.addPolygon(polygon[1]);
    m_ItemPath.closeSubpath();
    return true;

}

void QtPointROI::toGetItemVal()
{

    if(m_dScale != m_ControlList[0]->scale())
    {
        //很关键记录场景的缩放值
        m_dScale = m_ControlList[0]->scale();
        UpDate();
    }

    QPointF tPoint = this->mapToScene(m_Center);
    CMvPoint tCMvPoint;
    tCMvPoint.x = tPoint.x();
    tCMvPoint.y = tPoint.y();
    if(IsDataChange(tCMvPoint.Data(), m_CMvPoint.Data()))
    {
        m_CMvPoint = tCMvPoint;
        emit m_YtRoiShowDisp->ROIChange(m_CMvPoint.Data(), m_Key, m_types);
    }
}

QVector<double> QtPointROI::toGetDatavalue()
{
    QPointF tPoint = this->mapToScene(m_Center);
    CMvPoint tCMvPoint;
    tCMvPoint.x = tPoint.x();
    tCMvPoint.y = tPoint.y();
    return tCMvPoint.Data();
}

