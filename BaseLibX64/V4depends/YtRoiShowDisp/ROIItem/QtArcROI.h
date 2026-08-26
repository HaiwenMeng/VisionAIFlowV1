#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <QDebug>
#include <QGraphicsScene>
#include <qmath.h>
#include <QGraphicsSceneMouseEvent>
#include <QVariant>
#include "ytvisiondefine.h"
#include "BaseItem.h"

class QtArcROI : public BaseItem
{
    Q_OBJECT

public:
    QtArcROI(QVector<double> &tdata,QString &key);
    ~QtArcROI();
public:
    bool UpDate(int index=-1) override;
    void toGetItemVal();
    QVector<double> toGetDatavalue();

private:
    CMvArc m_CMvArc;
    double m_dStAngle;
    double m_dEdAngle;
    double m_dSpanAngle;
    double m_dRadius;
    QPointF Center;
    QPolygonF m_polygon;                              // 轮廓点集



};

