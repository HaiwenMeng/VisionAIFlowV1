#include "QtEllipseROI.h"
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QtMath>
#include <QDebug>
#include <QGraphicsTextItem>
#include "ytroishowdisp.h"

QtEllipseROI::QtEllipseROI(QVector<double> &tdata, QString &key)
{
    m_CMvEllipse.GetData(tdata);
    m_Key = key;
    m_ang = -m_CMvEllipse.angle;
    m_types = ellipseROI;
    QPointF set = this->mapFromScene(m_CMvEllipse.Center.x, m_CMvEllipse.Center.y);

    m_polygon << set + QPointF(-m_CMvEllipse.axisX, m_CMvEllipse.axisY)
            << set + QPointF(m_CMvEllipse.axisX, m_CMvEllipse.axisY)
            << set + QPointF(m_CMvEllipse.axisX, -m_CMvEllipse.axisY)
            << set + QPointF(-m_CMvEllipse.axisX, -m_CMvEllipse.axisY);

    m_ControlList << new ControlItem(this, QPointF(), 0, ControlItem::Move_Control);
    m_ControlList << new ControlItem(this, QPointF(), 1, ControlItem::Size_Control);
    m_ControlList << new ControlItem(this, QPointF(), 2, ControlItem::Size_Control);
    m_ControlList << new ControlItem(this, QPointF(), 3, ControlItem::Size_Control);
    m_ControlList << new ControlItem(this, QPointF(), 4, ControlItem::Size_Control);
    m_ControlList << new ControlItem(this, QPointF(), 5, ControlItem::Rotate_Control);
    UpDate();
}

QtEllipseROI::~QtEllipseROI()
{

}

bool QtEllipseROI::UpDate(int index)
{
    QPointF ptCenter = (m_polygon[0]+m_polygon[2]) / 2;
    if(index > 0) {
        QPointF Pf = m_ControlList[index]->GetPoint();
        QPointF pt2[2];
        QPointF ptOriginal = (m_polygon[1] + m_polygon[2]) / 2;
        switch (index)
        {
        case 1:
            m_polygon[0] = Pf;
            CalculateRectCornerPt(Pf, m_polygon[2], m_ang / 180 * MV_PI
, pt2);
            m_polygon[1] = pt2[1];
            m_polygon[3] = pt2[0];
            break;
        case 2:
            m_polygon[1] = Pf;
            CalculateRectCornerPt(Pf, m_polygon[3], m_ang / 180 * MV_PI
, pt2);
            m_polygon[0] = pt2[1];
            m_polygon[2] = pt2[0];
            break;
        case 3:
            m_polygon[2] = Pf;
            CalculateRectCornerPt(Pf, m_polygon[0], m_ang / 180 * MV_PI
, pt2);
            m_polygon[1] = pt2[0];
            m_polygon[3] = pt2[1];
            break;
        case 4:
            m_polygon[3] = Pf;
            CalculateRectCornerPt(Pf, m_polygon[1],m_ang / 180 * MV_PI
, pt2);
            m_polygon[0] = pt2[0];
            m_polygon[2] = pt2[1];
            break;
        case 5:
            AfterRotate(Pf, ptOriginal, ptCenter, m_polygon);
            m_ang = CalculateAngle((m_polygon[1] + m_polygon[2]) / 2, ptCenter);
            break;
        }
    }
    else {
        RotateMoveTo(m_ang / 180 * MV_PI
, ptCenter, m_polygon);
    }

    //prepareGeometryChange();

    m_ControlList[0]->SetPoint((m_polygon[0] + m_polygon[2]) / 2);
    m_ControlList[1]->SetPoint(m_polygon[0]);
    m_ControlList[2]->SetPoint(m_polygon[1]);
    m_ControlList[3]->SetPoint(m_polygon[2]);
    m_ControlList[4]->SetPoint(m_polygon[3]);
    m_ControlList[5]->SetPoint((m_polygon[1] + m_polygon[2]) / 2);
    m_ItemPath.clear();

    QTransform trans;
    trans.rotate(-m_ang);
    // 旋转到水平、画椭圆只能水平状态
    m_ItemPath.addEllipse(trans.map(m_polygon).boundingRect());
    // 旋转到正常角度
    trans.rotate(2 * m_ang);
    // 将画好的椭圆旋转到正常位置
    m_ItemPath = trans.map(m_ItemPath);
    m_ItemPath.closeSubpath();
    return true;
}

void QtEllipseROI::toGetItemVal()
{
    QPointF tP0,tP1,tP2,tP3;
    QLineF tLine02,tLine01,tLine03;

    //  0               1
    //
    //          5       4
    //
    //  3               2
    //
    tP0=this->mapToScene(m_polygon[0]);
    tP1=this->mapToScene(m_polygon[1]);
    tP2=this->mapToScene(m_polygon[2]);
    tP3=this->mapToScene(m_polygon[3]);
    tLine02=QLineF(tP0,tP2);
    tLine01=QLineF(tP0,tP1);
    tLine03=QLineF(tP0,tP3);

    CMvEllipse tCMvEllipse;
    tCMvEllipse.axisX=tLine01.length()/2;
    tCMvEllipse.axisY=tLine03.length()/2;
    tCMvEllipse.Center.x= tLine02.center().x();
    tCMvEllipse.Center.y= tLine02.center().y();
    tCMvEllipse.angle=-m_ang;
    if(IsDataChange(tCMvEllipse.Data(),m_CMvEllipse.Data()))
    {
        m_CMvEllipse=tCMvEllipse;
        emit m_YtRoiShowDisp->ROIChange(m_CMvEllipse.Data(),m_Key,m_types);
    }
}

QVector<double> QtEllipseROI::toGetDatavalue()
{
    return m_CMvEllipse.Data();
}
