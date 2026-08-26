#include "QtWaistShapeROI.h"
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QtMath>
#include <QGraphicsTextItem>
#include <QDebug>
#include "ytroishowdisp.h"

QtWaistShapeROI::QtWaistShapeROI(QVector<double> &tdata, QString &key)
{
    m_CMvWaistRound.GetData(tdata);
    m_Key = key;
    m_ang = 360-m_CMvWaistRound.angle;
    m_types = waistROI;
    QPointF set = this->mapFromScene(m_CMvWaistRound.Center.x, m_CMvWaistRound.Center.y);

    m_polygon << set + QPointF(-m_CMvWaistRound.cx, m_CMvWaistRound.cy)
              << set + QPointF(m_CMvWaistRound.cx ,m_CMvWaistRound.cy)
              << set + QPointF(m_CMvWaistRound.cx, -m_CMvWaistRound.cy)
              << set + QPointF(-m_CMvWaistRound.cx, -m_CMvWaistRound.cy);

    m_ControlList << new ControlItem(this, QPointF(), 0, ControlItem::Move_Control);
    m_ControlList << new ControlItem(this, QPointF(), 1, ControlItem::Size_Control);
    m_ControlList << new ControlItem(this, QPointF(), 2, ControlItem::Size_Control);
    m_ControlList << new ControlItem(this, QPointF(), 3, ControlItem::Size_Control);
    m_ControlList << new ControlItem(this, QPointF(), 4, ControlItem::Size_Control);
    m_ControlList << new ControlItem(this, QPointF(), 5, ControlItem::Rotate_Control);
    UpDate();
}

QtWaistShapeROI::~QtWaistShapeROI()
{

}
bool QtWaistShapeROI::UpDate(int index)
{
    QPointF ptCenter = (m_polygon[0]+m_polygon[2]) / 2;
    if(index>0) {
        QPointF Pf = m_ControlList[index]->GetPoint();
        QPointF pt2[2];
        QPointF ptOriginal = (m_polygon[1] + m_polygon[2]) / 2;
        switch (index) {
        case 1:
            m_polygon[0] = Pf;
            CalculateRectCornerPt(Pf, m_polygon[2], m_ang / 180 * MV_PI, pt2);
            m_polygon[1] = pt2[1];
            m_polygon[3] = pt2[0];
            break;
        case 2:
            m_polygon[1] = Pf;
            CalculateRectCornerPt(Pf, m_polygon[3], m_ang / 180 * MV_PI, pt2);
            m_polygon[0] = pt2[1];
            m_polygon[2] = pt2[0];
            break;
        case 3:
            m_polygon[2] = Pf;
            CalculateRectCornerPt(Pf, m_polygon[0], m_ang / 180 * MV_PI, pt2);
            m_polygon[1] = pt2[0];
            m_polygon[3] = pt2[1];
            break;
        case 4:
            m_polygon[3] = Pf;
            CalculateRectCornerPt(Pf, m_polygon[1], m_ang / 180 * MV_PI, pt2);
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
        RotateMoveTo(m_ang / 180 * MV_PI, ptCenter, m_polygon);
    }
    m_ControlList[0]->SetPoint((m_polygon[0]+m_polygon[2])/2);
    m_ControlList[1]->SetPoint(m_polygon[0]);
    m_ControlList[2]->SetPoint(m_polygon[1]);
    m_ControlList[3]->SetPoint(m_polygon[2]);
    m_ControlList[4]->SetPoint(m_polygon[3]);
    m_ControlList[5]->SetPoint((m_polygon[1]+m_polygon[2])/2);
    m_ItemPath.clear();
    QPainterPath tempath;
    QLineF tlineo3=QLineF(m_polygon[0],m_polygon[3]);
    QRectF rectMin(tlineo3.center().x() - tlineo3.length()/2,
                   tlineo3.center().y()- tlineo3.length()/2, tlineo3.length(), tlineo3.length());
    tempath.arcMoveTo(rectMin,90-m_ang);
    tempath.arcTo(rectMin, 90-m_ang, 180);

    m_ItemPath.addPath(tempath);
    tempath.clear();
    tlineo3=QLineF(m_polygon[1],m_polygon[2]);
    rectMin=QRectF(tlineo3.center().x() - tlineo3.length()/2,
                   tlineo3.center().y()- tlineo3.length()/2, tlineo3.length(), tlineo3.length());
    tempath.arcMoveTo(rectMin, 270-m_ang);
    tempath.arcTo(rectMin, 270-m_ang, 180);

    m_ItemPath.addPath(tempath);

    m_ItemPath.addPolygon(m_polygon);

    m_ItemPath.closeSubpath();
    return true;

}

void QtWaistShapeROI::toGetItemVal()
{
    QPointF tP0,tP1,tP2,tP3;
    QLineF tLine02,tLine01,tLine03;

    //  0               1
    //
    //          5       4
    //
    //  3               2
    //
    tP0 = this->mapToScene(m_polygon[0]);
    tP1 = this->mapToScene(m_polygon[1]);
    tP2 = this->mapToScene(m_polygon[2]);
    tP3 = this->mapToScene(m_polygon[3]);
    tLine02 = QLineF(tP0,tP2);
    tLine01 = QLineF(tP0,tP1);
    tLine03 = QLineF(tP0,tP3);

    CMvWaistRound tCMvRotatedRect;
    tCMvRotatedRect.cx = tLine01.length() / 2;
    tCMvRotatedRect.cy = tLine03.length() / 2;
    tCMvRotatedRect.Center.x = tLine02.center().x();
    tCMvRotatedRect.Center.y = tLine02.center().y();
    tCMvRotatedRect.angle =360 -m_ang;
    //qDebug() << "QtWaistShapeROI" << tCMvRotatedRect.angle;
    if(IsDataChange(tCMvRotatedRect.Data(),m_CMvWaistRound.Data())) {
        m_CMvWaistRound = tCMvRotatedRect;
        emit m_YtRoiShowDisp->ROIChange(m_CMvWaistRound.Data(),m_Key,m_types);
    }
}

QVector<double> QtWaistShapeROI::toGetDatavalue()
{
    return m_CMvWaistRound.Data();
}
