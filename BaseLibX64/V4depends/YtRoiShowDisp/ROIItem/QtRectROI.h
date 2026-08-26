#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOption>
#include <QGraphicsSceneEvent>
#include "ytvisiondefine.h"
#include "BaseItem.h"
class QtRectROI : public BaseItem
{
    Q_OBJECT
public:
    QtRectROI(QVector<double> &tdata,QString &key);
    ~QtRectROI();
public:
    CMvRect m_CMvRect;
public:
    bool UpDate(int index=-1) override;
    void toGetItemVal();
    QVector<double> toGetDatavalue();
public:
    QPolygonF m_polygon;                              // ÂÖÀªµã¼¯


};
