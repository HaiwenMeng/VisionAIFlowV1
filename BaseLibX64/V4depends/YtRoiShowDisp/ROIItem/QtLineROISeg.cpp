#include "QtLineROISeg.h"
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QtMath>
#include <QDebug>
#include <QGraphicsTextItem>
#include "ytroishowdisp.h"

QtLineROISeg::QtLineROISeg(QVector<double> &tdata, QString &key)
{
    m_Key = key;
    m_types = lineSegROI;
    m_CMvLineSeg.GetData(tdata);
    m_setScal=1;
    polygon << QPointF(m_CMvLineSeg.st.x, m_CMvLineSeg.st.y) << QPointF(m_CMvLineSeg.ed.x, m_CMvLineSeg.ed.y);
    double tGetAng=m_CMvLineSeg.Rad();
    Arrpolygon<<QPointF(m_CMvLineSeg.ed.x-m_setScal*30*cos(tGetAng-0.5), m_CMvLineSeg.ed.y-m_setScal*30*sin(tGetAng-0.5));
    Arrpolygon<<QPointF(m_CMvLineSeg.ed.x,m_CMvLineSeg.ed.y);
    Arrpolygon<<QPointF(m_CMvLineSeg.ed.x-m_setScal*30*cos(tGetAng+0.5), m_CMvLineSeg.ed.y-m_setScal*30*sin(tGetAng+0.5));

    Center = (polygon[0] + polygon[1]) / 2;
    Height = m_CMvLineSeg.width;
    m_ControlList << new ControlItem(this, QPointF(), 0, ControlItem::Move_Control);//中心
    m_ControlList << new ControlItem(this, QPointF(), 1, ControlItem::Size_Control);//起点
    m_ControlList << new ControlItem(this, QPointF(), 2, ControlItem::Size_Control);//终点

    UpDate();
}

QtLineROISeg::~QtLineROISeg()
{

}

bool QtLineROISeg::UpDate(int index)
{

    if(index>0)
    {
        if(index==1)
        {
            polygon[0]=m_ControlList[1]->GetPoint();//起点
        }else if(index==2)
        {
            polygon[1]=m_ControlList[2]->GetPoint();//终点
        }
        Center=(polygon[0]+polygon[1])/2;
    }

    m_ControlList[0]->SetPoint(Center);
    m_ControlList[1]->SetPoint(polygon[0]);
    m_ControlList[2]->SetPoint(polygon[1]);
    auto dt=polygon[0]-polygon[1];
    m_ItemPath.clear();
    qreal atn1=atan2(dt.y() ,dt.x());
    Arrpolygon.clear();
    Arrpolygon<<QPointF(polygon[1].x()+m_setScal*30*cos(atn1-0.5), polygon[1].y()+m_setScal*30*sin(atn1-0.5));
    Arrpolygon<<QPointF(polygon[1].x(),polygon[1].y());
    Arrpolygon<<QPointF(polygon[1].x()+m_setScal*30*cos(atn1+0.5), polygon[1].y()+m_setScal*30*sin(atn1+0.5));
    m_ItemPath.addPolygon(Arrpolygon);
    m_ItemPath.addPolygon(polygon);

    return true;
}

void QtLineROISeg::toGetItemVal()
{
    if(m_setScal != m_ControlList[0]->scale())
    {
        //很关键记录场景的缩放值
        m_setScal = m_ControlList[0]->scale();
        UpDate(-1);
    }

    QPointF tPointst = this->mapToScene(polygon[0]);
    QPointF tPointed = this->mapToScene(polygon[1]);
    CMvLineSeg tLineSeg;
    tLineSeg.st.x = tPointst.x();
    tLineSeg.st.y = tPointst.y();
    tLineSeg.ed.x = tPointed.x();
    tLineSeg.ed.y = tPointed.y();
    tLineSeg.width = Height;
    if(IsDataChange(tLineSeg.Data(), m_CMvLineSeg.Data())) {
        m_CMvLineSeg = tLineSeg;
        emit m_YtRoiShowDisp->ROIChange(m_CMvLineSeg.Data(), m_Key, m_types);
    }
}

QVector<double> QtLineROISeg::toGetDatavalue()
{
    return m_CMvLineSeg.Data();
}
