#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOption>
#include <QGraphicsSceneEvent>
#include "ytvisiondefine.h"
#include "BaseItem.h"
class QtPointROI : public BaseItem
{
    Q_OBJECT

public:
    explicit QtPointROI(QVector<double> &tdata,QString &key);
    ~QtPointROI();
public:
    CMvPoint m_CMvPoint;
public:
    bool UpDate(int index=-1) override;
    void toGetItemVal();
    QVector<double> toGetDatavalue();

public:
    double     m_dScale=1;
    QPointF    m_Center;                                 // ÖÐÐÄµã



};
