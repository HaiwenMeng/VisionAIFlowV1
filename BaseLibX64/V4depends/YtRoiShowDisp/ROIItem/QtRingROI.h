#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsScene>
#include <QtMath>
#include <QDebug>
#include "ytvisiondefine.h"
#include "BaseItem.h"

class QtRingROI : public BaseItem
{
    Q_OBJECT

public:
    QtRingROI(QVector<double> &tdata,QString &key);
    ~QtRingROI();
public:
    bool UpDate(int index=-1) override;
    void toGetItemVal();
    QVector<double> toGetDatavalue();
public:
    CMvRingCircle m_CMvRingCircle;
    qreal RadiusMin;
    qreal RadiusMax;
    QPointF    m_Center;                                 // 中心点

};

