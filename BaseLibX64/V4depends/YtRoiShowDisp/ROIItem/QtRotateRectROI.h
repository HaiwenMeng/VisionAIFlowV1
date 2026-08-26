#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOption>
#include <QGraphicsSceneEvent>
#include <QCursor>
#include "ytvisiondefine.h"
#include "BaseItem.h"
class QtRotateRectROI : public BaseItem
{
    Q_OBJECT

public:
    QtRotateRectROI(QVector<double> &tdata,QString &key);
    ~QtRotateRectROI();
public:
    bool UpDate(int index=-1) override;
    void toGetItemVal();
    QVector<double> toGetDatavalue();
public:
    CMvRotatedRect m_CMvRotatedRect;
    double m_ang;
    QPolygonF m_polygon;                              // ÂÖÀªµã¼¯


};
