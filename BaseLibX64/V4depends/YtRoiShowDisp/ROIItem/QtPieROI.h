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

class QtPieROI : public BaseItem
{
    Q_OBJECT

public:
    QtPieROI(QVector<double> &tdata,QString &key);
    ~QtPieROI();
public:
    bool UpDate(int index=-1) override;
    void toGetItemVal();
    QVector<double> toGetDatavalue();

private:
    CMvPie m_CMvPie;
    double m_dStAngle;
    double m_dEdAngle;
    double m_dSpanAngle;
    double m_dRadisMin;
    double m_dRadisMax;
    QPolygonF m_lastPolygon;
    QPolygonF m_Polygon;
    QPointF Center;


};

