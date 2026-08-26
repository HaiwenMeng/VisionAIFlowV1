#ifndef CONTROLITEM_H
#define CONTROLITEM_H
/************************************************************************
  * @auther         :
  * @date           : 2021-4
  * @description    : 控制点
  * ROI模块中美的控制点
  * 中心点ju'xin矩形 点击后可移动
  * 控制点圆形、点击后可以拖动
************************************************************************/

#include <QObject>
#include <QAbstractGraphicsShapeItem>
#include <QPointF>
#include <QPen>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QCursor>
#include <QKeyEvent>
#include <QList>
#include <QDebug>

class ControlItem : public QObject, public QAbstractGraphicsShapeItem
{
    Q_OBJECT
public:
    enum ControlType {
        Move_Control = 0,    // 移动控制
        Size_Control,        // 大小控制
        Rotate_Control       // 旋转控制
    };

    explicit ControlItem(QGraphicsItemGroup* parent, QPointF p, int nPointIndex, int nControlType = 0);
    QPointF GetPoint();
    QPointF GetBefPoint();
    void SetPoint(QPointF p);
    double dX();
    double dY();
protected:
    virtual QRectF boundingRect() const override;
    virtual void paint(QPainter *painter,const QStyleOptionGraphicsItem *option,QWidget *widget) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;


private:
    QPainterPath m_ItemPath;
    QPen m_Pen;
    QPointF m_pointPos;
    int m_nPointIndex = 0;
    int m_nControlType = 0;
    int m_nItemSize = 8;

    double m_dx;
    double m_dy;
    QPointF m_BefPoint;
};

#endif // CONTROLITEM_H

