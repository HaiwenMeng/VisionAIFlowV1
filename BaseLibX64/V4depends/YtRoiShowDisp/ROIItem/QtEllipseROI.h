#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOption>
#include <QGraphicsSceneEvent>
#include <QCursor>
#include <QMenu>
#include <QVariant>
#include "ytvisiondefine.h"
#include "BaseItem.h"

class QtEllipseROI : public BaseItem
{
    Q_OBJECT

public:
    QtEllipseROI(QVector<double> &tdata,QString &key);
    ~QtEllipseROI();
public:
    bool UpDate(int index=-1) override;
    void toGetItemVal();
    QVector<double> toGetDatavalue();
public:
    CMvEllipse m_CMvEllipse;
    double m_ang;
    QPointF m_Center;
    QPolygonF m_polygon;                              // ÂÖÀªµã¼¯


};
