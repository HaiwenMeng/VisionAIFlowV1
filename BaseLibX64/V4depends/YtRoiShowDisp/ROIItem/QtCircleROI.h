#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOption>
#include <QGraphicsSceneEvent>
#include "ytvisiondefine.h"
#include "BaseItem.h"

class QtCircleROI : public BaseItem
{
    Q_OBJECT
public:
    QtCircleROI(QVector<double> &tdata, QString &key);
    ~QtCircleROI();
public:
    CMvCircle m_CMvCircle;
public:
    bool UpDate(int index=-1) override;
    void toGetItemVal();
    QVector<double> toGetDatavalue();

private:
    double m_dRadius;
    QPointF Center;

};
