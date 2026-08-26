#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOption>
#include <QGraphicsSceneEvent>
#include "ytvisiondefine.h"
#include "BaseItem.h"

class QtLineROI : public BaseItem
{
    Q_OBJECT
public:
    QtLineROI(QVector<double> &tdata,QString &key);
    ~QtLineROI();
public:
    CMvLine m_CMvLine;
public:
    bool UpDate(int index=-1) override;
    void toGetItemVal();
    QVector<double> toGetDatavalue();
public:
    CMvLineSeg m_CMvLineSeg;
    QPointF P1Glob;
    QPointF P2Glob;
    QPointF P1;
    QPointF P2;
    qreal Height;
    qreal angle;
    qreal Lenth;
    QPointF Center;
    QPolygonF polygon;                              // ÂÖÀªµã¼¯
    QPolygonF Arrpolygon;                              // ¼ýÍ·
    qreal m_setScal;



};

