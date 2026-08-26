#ifndef BASEITEM_H
#define BASEITEM_H
#include "ControlItem.h"
#include "ytvisiondefine.h"
#include <QObject>
#include <QList>
class YtRoiShowDisp;
#define YtPointControlSize 8

bool IsDataChange(QVector<double> InAdata, QVector<double> InBData);
template <typename T>    //T 为QPointF 或QPoint
double Distance(T p1, T p2);

// 点到线的垂点
double PointToLineDistance(QPointF pt, CMvLineSeg line);

double CalculateAngle(QPointF point1, QPointF point2);
// 矩形角点计算
void CalculateRectCornerPt(QPointF ptMove, QPointF ptNoMove, double dAngle, QPointF point[2]);
// 弧形角点计算
void CalculateArcCornerPt(QPointF ptMove, QPointF ptCenter, int nControlIndex, QPolygonF& polygon);

void AfterRotate(QPointF ptMove, QPointF ptOriginal, QPointF ptCenter, QPolygonF &polygon);

void RotateMoveTo(double dAngle, QPointF ptCenter, QPolygonF &polygon, int nPolygonId = -1);
//有向线段夹角
double VectorAngle(double v1x, double v1y, double v2x, double v2y);
//
//**************************************基类***************************************************
class BaseItem : public QObject, public QGraphicsItemGroup
{
    Q_OBJECT
    Q_PROPERTY(qreal ZoomVal WRITE SetZoomVal)
public:
    void SetZoomVal(qreal ZoomVal);
    virtual bool UpDate(int index=-1) = NULL;
    void paint(QPainter *painter,const QStyleOptionGraphicsItem *option,QWidget *widget);
    virtual void toGetItemVal() = NULL;
    virtual QVector<double> toGetDatavalue() = NULL;
    void toSetParent(YtRoiShowDisp *SetPa=nullptr);

public:
    BaseItem();
    virtual ~BaseItem();
    virtual QRectF boundingRect() const override;
protected:
    QList<ControlItem* > m_ControlList;
    qreal m_scaler;                                   // 缩放系数
    int m_nControlItemSize = YtPointControlSize;               // 控制点尺寸
    QColor m_ItemColor = Qt::blue;         // 线条颜色
    QPainterPath m_ItemPath;                          // 有边框区域
public:
    int m_types;                 // 枚举类型
    QString m_Key;
    YtRoiShowDisp *m_YtRoiShowDisp;

};


#endif // BASEITEM_H

