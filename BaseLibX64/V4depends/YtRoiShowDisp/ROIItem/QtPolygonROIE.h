#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOption>
#include <QGraphicsSceneEvent>
#include <QCursor>
#include "ytvisiondefine.h"
#include <QMenu>
#include "BaseItem.h"

class QtPolygonROIE : public BaseItem
{
    Q_OBJECT
public:
    explicit QtPolygonROIE(QVector<double> &tdata,QString &key);
    ~QtPolygonROIE();
public:
    bool UpDate(int index=-1) override;
    void toGetItemVal();
    QVector<double> toGetDatavalue();
 QRectF boundingRect() const override;
private:
    void addPoint(QPointF point);

    void delPoint(int nIndex);

    int indexOfPointInPolygon(QPointF pf);

private slots:
    void slot_menuTrigger(QAction *action);

protected:
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);

private:
    CMvPolygon m_CMvPolygon;
    QMenu *m_pMenu;
    QPointF m_ptMousePress;
    int m_nControlItemIndex;
    QPolygonF polygon;                              // ÂÖÀªµã¼¯
    double     m_dScale=1;

};

