#include "BaseItem.h"
#include <math.h>
#include <QGraphicsView>
#include <QtDebug>
#include "ytroishowdisp.h"
#define PI 3.141592653

bool IsDataChange(QVector<double> InAdata, QVector<double> InBData)
{
    if(InAdata.size()!=InBData.size())
    {
        return true;
    }
    for(int i=0;i<InAdata.size();i++)
    {
        if(abs(InAdata[i]-InBData[i])>0.5)
        {
            return true;
        }
    }
    return false;
}

template <typename T>    //T 为QPointF 或QPoint
double Distance(T p1, T p2)
{
    return sqrt(pow(p1.x() - p2.x(), 2) + pow(p1.y() - p2.y(), 2));
}

double PointToLineDistance(QPointF pt, CMvLineSeg line)
{
    double cross = (line.ed.x - line.st.x) * (pt.x() - line.st.x) + (line.ed.y - line.st.y) * (pt.y() - line.st.y);
    if(cross <= 0) {
        return sqrt((pt.x() - line.st.x) * (pt.x() - line.st.x) + (pt.y() - line.st.y) * (pt.y() - line.st.y));
    }

    double d2 = (line.ed.x - line.st.x) * (line.ed.x - line.st.x) + (line.ed.y - line.st.y) * (line.ed.y - line.st.y);
    if (cross >= d2)  {
        return sqrt((pt.x() - line.ed.x) * (pt.x() - line.ed.x) + (pt.y() - line.ed.y) * (pt.y() - line.ed.y));
    }

    double r = cross / d2;
    double px = line.st.x + (line.ed.x - line.st.x) * r;
    double py = line.st.y + (line.ed.y - line.st.y) * r;
    return sqrt((pt.x() - px) * (pt.x() - px) + (py - pt.y()) * (py - pt.y()));
}

double CalculateAngle(QPointF point1, QPointF point2)
{
    double dAngle = (atan2(point1.y() - point2.y(), point1.x() - point2.x())) / MV_PI * 180;
    if(dAngle >= 0) {
        return dAngle;
    }
    else {
        return 360 - abs(dAngle);
    }
}

void CalculateRectCornerPt(QPointF ptMove, QPointF ptNoMove, double dAngle, QPointF point[2])
{
    double dist = 0, tempDist = 0;
    double x1 = 0, y1 = 0;
    double sita = 0;
    dist = Distance(ptMove, ptNoMove);
    sita = atan2(ptMove.y() - ptNoMove.y(), ptMove.x() - ptNoMove.x());
    sita -= dAngle;
    tempDist = dist*cos(sita);
    x1 = ptNoMove.x() + tempDist*cos(dAngle);
    y1 = ptNoMove.y() + tempDist*sin(dAngle);
    point[0] = QPointF(x1, y1);
    point[1] = ptMove + ptNoMove - point[0];
}

void CalculateArcCornerPt(QPointF ptMove, QPointF ptCenter, int nControlIndex, QPolygonF &polygon)
{
    QPolygonF orignalPoly = polygon;
    QPointF ptOrignal = polygon[nControlIndex - 1];
    double sita1 = atan2(ptOrignal.y() - ptCenter.y(), ptOrignal.x() - ptCenter.x());
    double sita2 = atan2(ptMove.y() - ptCenter.y(), ptMove.x() - ptCenter.x());
    double deltaSita = sita2 - sita1;
    double sita, distance;
    distance = Distance(ptMove, ptCenter);
    for (int i = 0; i < polygon.size(); i++) {
        sita = atan2(orignalPoly[i].y() - ptCenter.y(), orignalPoly[i].x() - ptCenter.x());

        if((i == 1) || (i == 3)) {
            sita += deltaSita;
        }

        if((i != nControlIndex - 2) && (i != nControlIndex - 1)) {
            double distance2 = Distance(polygon[i], ptCenter);
            polygon[i].setX(distance2 * cos(sita) + ptCenter.x());
            polygon[i].setY(distance2 * sin(sita) + ptCenter.y());
        }
        else {
            polygon[i].setX(distance * cos(sita) + ptCenter.x());
            polygon[i].setY(distance * sin(sita) + ptCenter.y());
        }
    }
}

void AfterRotate(QPointF ptMove, QPointF ptOriginal, QPointF ptCenter, QPolygonF &polygon)
{
    QPolygonF orignalPoly = polygon;
    double sita1 = atan2(ptOriginal.y() - ptCenter.y(), ptOriginal.x() - ptCenter.x());
    double sita2 = atan2(ptMove.y() - ptCenter.y(), ptMove.x() - ptCenter.x());
    double deltaSita = sita2 - sita1;
    double sita, distance;
    for (int i = 0; i < polygon.size(); i++) {
        sita = atan2(orignalPoly[i].y() - ptCenter.y(), orignalPoly[i].x() - ptCenter.x());
        sita += deltaSita;
        distance = Distance(orignalPoly[i], ptCenter);
        polygon[i].setX(distance * cos(sita) + ptCenter.x());
        polygon[i].setY(distance * sin(sita) + ptCenter.y());
    }
}

void RotateMoveTo(double dAngle, QPointF ptCenter, QPolygonF &polygon, int nPolygonId)
{
    QPolygonF orignalPoly = polygon;
    double distance;
    for (int i = 0; i < polygon.size(); i++) {
        if((i != nPolygonId) && (nPolygonId != -1)) {
            continue;
        }
        double sita = atan2(orignalPoly[i].y() - ptCenter.y(), orignalPoly[i].x() - ptCenter.x());
        sita += dAngle;
        distance = Distance(orignalPoly[i], ptCenter);
        polygon[i].setX(distance * cos(sita) + ptCenter.x());
        polygon[i].setY(distance * sin(sita) + ptCenter.y());
    }
}

void BaseItem::SetZoomVal(qreal ZoomVal)
{
    qDebug()<<"BaseItem::SetZoomVal"<<m_scaler<<ZoomVal;

    m_scaler = ZoomVal;
}

void BaseItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    if(m_ControlList.isEmpty()) {
        return;
    }
    // 缩放控制点尺寸
    for(int i = 0; i < m_ControlList.size(); i++)
    {
        // 无判断时会进入死循环
        if(m_ControlList[i]->scale() != m_scaler)
        {
            m_ControlList[i]->setScale(m_scaler);
        }
    }
    //绘制ROI的文本描述
    auto pen = painter->pen();

    painter->setFont(QFont(u8"微软雅黑",YtMAX(2.0,12*m_scaler),YtMAX(2.0,12*m_scaler)));
    pen.setColor(Qt::red);
    painter->setPen(pen);
    painter->drawText(m_ControlList[0]->GetPoint(), m_Key);

    pen.setWidthF(2 * m_scaler);
    pen.setColor(m_ItemColor);
    painter->setPen(pen);
    painter->drawPath(m_ItemPath);
    toGetItemVal();
}

void BaseItem::toSetParent(YtRoiShowDisp *SetPa)
{
    m_YtRoiShowDisp=SetPa;
}



BaseItem::BaseItem()
{
    // 设置后才能将事件传递到子元素
    setHandlesChildEvents(false);
    // 设置为可选中、可移动、可设定焦点
    // 设置为不可选中、不可移动、不可设定焦点
    setFlags(flags()&~ItemIsSelectable |
             flags()&~ItemIsMovable |
             flags()&~ItemIsFocusable);
}

BaseItem::~BaseItem()
{
    for(int i=0;i<m_ControlList.size();i++)
    {
        m_ControlList[i]->deleteLater();
    }
    m_ControlList.clear();

}

QRectF BaseItem::boundingRect() const
{
    return m_ItemPath.boundingRect();
}

double VectorAngle(double v1x, double v1y, double v2x, double v2y)
{
    double ret = 0.0;
    double l1, l2;
    double err = 0.00001;
    l1 = sqrt(v1x * v1x + v1y * v1y);
    l2 = sqrt(v2x * v2x + v2y * v2y);
    if((l1 > err) && (l2 > err))
    {
        ret = acos((v1x * v2x + v1y * v2y) / (l1 * l2));
    }
    return ret;

}
