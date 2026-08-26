#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOption>
#include <QGraphicsSceneEvent>
#include <QCursor>
#include "ytvisiondefine.h"
#include "BaseItem.h"
class QtWaistShapeROI : public BaseItem
{
    Q_OBJECT

public:
    QtWaistShapeROI(QVector<double> &tdata,QString &key);
    ~QtWaistShapeROI();
public:
    bool UpDate(int index=-1) override;
    void toGetItemVal();
    QVector<double> toGetDatavalue();
public:
    CMvWaistRound m_CMvWaistRound;
    double m_ang;
    QPolygonF m_polygon;                              // ÂÖÀªµã¼¯


};
