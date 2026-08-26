#include "QtPolygonROIE.h"
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QtMath>
#include <QDebug>
#include <QGraphicsTextItem>
#include <QAction>
#include <QMessageBox>
#include "ytroishowdisp.h"

QtPolygonROIE::QtPolygonROIE(QVector<double> &tdata, QString &key)
{
    m_CMvPolygon.GetData(tdata);
    m_Key = key;
    m_ptMousePress = QPointF(0, 0);
    m_nControlItemIndex = 0;
    m_types = polygonROIE;
    m_dScale = this->scale();

    m_ControlList << new ControlItem(this, QPointF(), 0, ControlItem::Move_Control);
    for (int i = 0; i < m_CMvPolygon.points.size(); i++)
    {
        QPointF point = QPointF(m_CMvPolygon.points.at(i).x, m_CMvPolygon.points.at(i).y);
        polygon << point;
        m_ControlList << new ControlItem(this, QPointF(), i + 1, ControlItem::Size_Control);
    }
    UpDate();

    m_pMenu = new QMenu;
    connect(m_pMenu, &QMenu::triggered, this, &QtPolygonROIE::slot_menuTrigger);
    m_pMenu->addAction(QString(u8"添加顶点"));
    m_pMenu->addAction(QString(u8"删除顶点"));
}

QtPolygonROIE::~QtPolygonROIE()
{
}

bool QtPolygonROIE::UpDate(int index)
{
    if (index > 0)
    {
        polygon[index - 1] = m_ControlList[index]->GetPoint();
    }

    QPointF ptCenter(0, 0);
    for (int i = 0; i < polygon.size(); i++)
    {
        m_ControlList[i + 1]->SetPoint(polygon[i]);
        ptCenter += polygon[i];
    }

    if (polygon.size() > 0)
    {
        ptCenter = ptCenter / polygon.size();
    }
    m_ControlList[0]->SetPoint(ptCenter);

    m_ItemPath.clear();
    for (int i = 0; i < polygon.size(); i++)
    {
        if ((i + 1) >= polygon.size())
        {
            continue;
        }
        QPolygonF tmpPolygon;
        tmpPolygon.append(polygon[i]);
        tmpPolygon.append(polygon[i + 1]);
        m_ItemPath.addPolygon(tmpPolygon);
    }
    QFont tFont("Times", YtMAX(2.0, 30 * m_dScale), YtMAX(2.0, 30 * m_dScale));
    for (int i = 0; i < polygon.size(); i++)
    {
        m_ItemPath.addText(polygon[i], tFont, QString::number(i + 1));
    }
    m_ItemPath.closeSubpath();
    return true;
}

void QtPolygonROIE::toGetItemVal()
{
    if (m_dScale != m_ControlList[0]->scale())
    {
        //很关键记录场景的缩放值
        m_dScale = m_ControlList[0]->scale();
        UpDate();
    }

    CMvPolygon tCMvPolygon;
    QVector<double> vecData;
    vecData.clear();
    foreach (QPointF pt, polygon)
    {
        QPointF tPoint = this->mapToScene(pt);
        vecData << tPoint.x() << tPoint.y();
    }
    tCMvPolygon.GetData(vecData);
    if (IsDataChange(tCMvPolygon.Data(), m_CMvPolygon.Data()))
    {
        m_CMvPolygon = tCMvPolygon;
        emit m_YtRoiShowDisp->ROIChange(m_CMvPolygon.Data(), m_Key, m_types);
    }
}

QVector<double> QtPolygonROIE::toGetDatavalue()
{
    return m_CMvPolygon.Data();
}

QRectF QtPolygonROIE::boundingRect() const
{
    return m_ItemPath.boundingRect()
        .united(polygon.boundingRect())
        .marginsAdded(QMarginsF(100 * m_scaler, 100 * m_scaler, 100 * m_scaler, 100 * m_scaler));
}

void QtPolygonROIE::addPoint(QPointF point)
{
    int nInsetIndex = 0;
    double dMinDistance = FLT_MAX;
    for (int i = 0; i < polygon.size(); i++)
    {
        if ((i + 1) > polygon.size())
        {
            continue;
        }
        CMvLineSeg line;
        if ((i + 1) == polygon.size())
        {
            line = CMvLineSeg(polygon[i].x(), polygon[i].y(), polygon[0].x(), polygon[0].y());
            ;
        }
        else
        {
            line = CMvLineSeg(polygon[i].x(), polygon[i].y(), polygon[i + 1].x(), polygon[i + 1].y());
        }

        double dDistance = PointToLineDistance(point, line);
        if (dMinDistance > dDistance)
        {
            dMinDistance = dDistance;
            nInsetIndex = i;
        }
    }

    if (nInsetIndex == (polygon.size() - 1))
    {
        if (Distance(point, polygon.first()) < Distance(point, polygon.last()))
        {
            nInsetIndex = -1;
        }
    }

    QPolygonF tmpPolygon = polygon;
    int nPolygonSize = polygon.size();
    polygon.clear();
    for (int i = 0; i < nPolygonSize + 1; i++)
    {
        if (i <= nInsetIndex)
        {
            polygon << tmpPolygon[i];
        }
        else if (i == (nInsetIndex + 1))
        {
            polygon << point;
        }
        else
        {
            polygon << tmpPolygon[i - 1];
        }
    }
}

void QtPolygonROIE::delPoint(int nIndex)
{
    polygon.remove(nIndex - 1);
}

int QtPolygonROIE::indexOfPointInPolygon(QPointF pf)
{
    for (int i = 0; i < polygon.size(); i++)
    {
        QRectF rect = QRectF(polygon[i].x() - m_nControlItemSize * m_scaler,
                             polygon[i].y() - m_nControlItemSize * m_scaler,
                             m_nControlItemSize * m_scaler * 2,
                             m_nControlItemSize * m_scaler * 2);
        if (rect.contains(pf))
            return i + 1;
    }
    return -1;
}

void QtPolygonROIE::slot_menuTrigger(QAction *action)
{
    if (action->text() == QString(u8"添加顶点"))
    {
        addPoint(m_ptMousePress);
    }
    else if (action->text() == QString(u8"删除顶点"))
    {
        if (polygon.size() <= 2)
        {
            QMessageBox::information(nullptr, QString(u8"系统提示"), QString(u8"无法删除，至少需要两个顶点"));
            return;
        }
        delPoint(m_nControlItemIndex);
    }

    // 重新创建
    foreach (ControlItem *pControlItem, m_ControlList)
    {
        if (pControlItem != nullptr)
        {
            delete pControlItem;
            pControlItem = nullptr;
        }
    }
    m_ControlList.clear();
    m_ControlList << new ControlItem(this, QPointF(), 0, ControlItem::Move_Control);
    for (int j = 0; j < polygon.size(); j++)
    {
        m_ControlList << new ControlItem(this, QPointF(), j + 1, ControlItem::Size_Control);
    }

    UpDate();
}

void QtPolygonROIE::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
    {
        m_ptMousePress = event->pos();
        m_nControlItemIndex = indexOfPointInPolygon(event->pos());
        if (m_nControlItemIndex == -1)
        {
            m_pMenu->actions()[0]->setVisible(true);
            m_pMenu->actions()[1]->setVisible(false);
        }
        else
        {
            m_pMenu->actions()[0]->setVisible(false);
            m_pMenu->actions()[1]->setVisible(true);
        }
        m_pMenu->exec(event->screenPos());
    }
    QGraphicsItem::mousePressEvent(event);
}
