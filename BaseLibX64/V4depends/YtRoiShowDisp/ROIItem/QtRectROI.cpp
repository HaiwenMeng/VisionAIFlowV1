#include "QtRectROI.h"
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QtMath>
#include <QGraphicsTextItem>
#include <QDebug>
#include "ytroishowdisp.h"
QtRectROI::QtRectROI(QVector<double> &tdata, QString &key)
{
    m_CMvRect.GetData(tdata);
    m_Key = key;
    m_types = rectangleROI;
    QPointF set = this->mapFromScene(m_CMvRect.LeftTop.x, m_CMvRect.LeftTop.y);
    m_polygon.append(set);
    m_polygon.append(set + QPointF(m_CMvRect.cx, 0));
    m_polygon.append(set + QPointF(m_CMvRect.cx, m_CMvRect.cy));
    m_polygon.append(set + QPointF(0, m_CMvRect.cy));
    m_ControlList << new ControlItem(this, QPointF(), 0, ControlItem::Move_Control);
    m_ControlList << new ControlItem(this, QPointF(), 1, ControlItem::Size_Control);
    m_ControlList << new ControlItem(this, QPointF(), 2, ControlItem::Size_Control);
    m_ControlList << new ControlItem(this, QPointF(), 3, ControlItem::Size_Control);
    m_ControlList << new ControlItem(this, QPointF(), 4, ControlItem::Size_Control);
    UpDate();
}

QtRectROI::~QtRectROI()
{

}

bool QtRectROI::UpDate(int index)
{
    if(index>0)
    {
        //需要更新角点的情况
        QPointF Pf=m_ControlList[index]->GetPoint();
        if(index-1<4)
        {
            m_polygon[index-1]=Pf;
        }

        //角点分情况 变更
        switch (index)
        {
        case 1:
            m_polygon[1].setY(Pf.y());
            m_polygon[3].setX(Pf.x());
            break;
        case 2:
            m_polygon[0].setY(Pf.y());
            m_polygon[2].setX(Pf.x());
            break;
        case 3:
            m_polygon[3].setY(Pf.y());
            m_polygon[1].setX(Pf.x());
            break;
        case 4:
            m_polygon[2].setY(Pf.y());
            m_polygon[0].setX(Pf.x());
            break;
        default:
            break;
        }
    }
    //中心点变更
    m_ControlList[0]->SetPoint((m_polygon[0]+m_polygon[2])/2);
    m_ControlList[1]->SetPoint(m_polygon[0]);
    m_ControlList[2]->SetPoint(m_polygon[1]);
    m_ControlList[3]->SetPoint(m_polygon[2]);
    m_ControlList[4]->SetPoint(m_polygon[3]);
    m_ItemPath.clear();
    m_ItemPath.addPolygon(m_polygon);
    m_ItemPath.closeSubpath();
    return true;
}

void QtRectROI::toGetItemVal()
{
    CMvRect tCMvRect;
    QPointF tP0,tP2,tP1,tP3;
    tP0 = this->mapToScene(m_polygon[0]);
    tP2 = this->mapToScene(m_polygon[2]);
    tP1 = this->mapToScene(m_polygon[1]);
    tP3 = this->mapToScene(m_polygon[3]);

    tCMvRect.LeftTop.x = YtMIN(YtMIN(tP0.x(),tP2.x()),YtMIN(tP1.x(),tP3.x()));
    tCMvRect.LeftTop.y =  YtMIN(YtMIN(tP0.y(),tP2.y()),YtMIN(tP1.y(),tP3.y()));
    tCMvRect.cx = abs(tP2.x() - tP0.x());
    tCMvRect.cy = abs(tP2.y() - tP0.y());

    if(IsDataChange(tCMvRect.Data(),m_CMvRect.Data())) {
        m_CMvRect = tCMvRect;
        emit m_YtRoiShowDisp->ROIChange(m_CMvRect.Data(),m_Key,m_types);
    }
}

QVector<double> QtRectROI::toGetDatavalue()
{
    return m_CMvRect.Data();

}


