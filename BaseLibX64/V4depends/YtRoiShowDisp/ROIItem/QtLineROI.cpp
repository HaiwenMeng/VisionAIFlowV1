#include "QtLineROI.h"
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QtMath>
#include <QDebug>
#include <QGraphicsTextItem>
#include "ytroishowdisp.h"

QtLineROI::QtLineROI(QVector<double> &tdata, QString &key)
{
    m_Key = key;
    m_types = lineSegCabROI;
    m_CMvLineSeg.GetData(tdata);
    m_setScal=this->scale();
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
    m_ControlList << new ControlItem(this, QPointF(), 3, ControlItem::Size_Control);//宽度
    //
    auto dt=polygon[0]-polygon[1];
    Lenth=YtDist2P(dt.x(),dt.y(),0,0);
    angle = atan2(dt.y(), -dt.x());;
    qreal s = sin(angle);
    qreal c = cos(angle);

    qDebug()<<Height<<"ZZZZZZZZZZZZZZ";
    m_ControlList[3]->SetPoint(Center+QPointF(s*Height,c*Height));
    UpDate();
}

QtLineROI::~QtLineROI()
{

}

bool QtLineROI::UpDate(int index)
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
        else if(index==3)
        {
            QPointF Pf=m_ControlList[3]->GetPoint();
            Height=PointToLineDistance(Pf,CMvLineSeg(polygon[0].x(),polygon[0].y(),polygon[1].x(),polygon[1].y()));
            QLineF temLine;
            QLineF temOrLine;
            temLine.setP1(polygon[0]);
            temLine.setP2(polygon[1]);
            temOrLine.setP1(polygon[0]);
            temOrLine.setP2(Pf);
            //
            double angoffset=temLine.angle()-temOrLine.angle();
            //qDebug()<<temLine.angle()-temOrLine.angle()<<"PPPP";
            int tSetAng=int(angoffset+720)%360;
            qDebug()<<tSetAng<<"PPPP";
            if(tSetAng>=180)
            {
                Height=-Height;
            }
        }
        Center=(polygon[0]+polygon[1])/2;
    }

    m_ControlList[0]->SetPoint(Center);
    m_ControlList[1]->SetPoint(polygon[0]);
    m_ControlList[2]->SetPoint(polygon[1]);
    auto dt=polygon[0]-polygon[1];
    Lenth=YtDist2P(dt.x(),dt.y(),0,0);
    angle = atan2(dt.y(), -dt.x());;
    qreal s = sin(angle);
    qreal c = cos(angle);
    ///

   m_ControlList[3]->SetPoint(Center+QPointF(s*Height,c*Height));
    qreal atn1=atan2(dt.y() ,dt.x());
    m_ItemPath.clear();
    Arrpolygon.clear();
    Arrpolygon<<QPointF(polygon[1].x()+m_setScal*30*cos(atn1-0.5), polygon[1].y()+m_setScal*30*sin(atn1-0.5));
    Arrpolygon<<QPointF(polygon[1].x(),polygon[1].y());
    Arrpolygon<<QPointF(polygon[1].x()+m_setScal*30*cos(atn1+0.5), polygon[1].y()+m_setScal*30*sin(atn1+0.5));

    m_ItemPath.addPolygon(Arrpolygon);
    m_ItemPath.addPolygon(polygon);
    QPolygonF tSetWidtploy;                              // 箭头
    QPointF SetPos;
    tSetWidtploy<<(Center+QPointF(s*Height,c*Height));
    tSetWidtploy<<(Center-QPointF(s*Height,c*Height));
    int seti=1;
    if(Height<0)
    {
        seti=-1;
    }

    tSetWidtploy<<(Center-QPointF(s*Height,c*Height)-seti*QPointF(m_setScal*10*cos(atn1-0.5+MV_PI_2),m_setScal*10*sin(atn1+MV_PI_2-0.5)));

    tSetWidtploy<<(Center-QPointF(s*Height,c*Height));
    tSetWidtploy<<(Center-QPointF(s*Height,c*Height)+seti*QPointF(m_setScal*10*cos(atn1-MV_PI_2+0.5),m_setScal*10*sin(atn1-MV_PI_2+0.5)));
    m_ItemPath.addPolygon(tSetWidtploy);
    tSetWidtploy.clear();
    SetPos=(polygon[0]+Center)/2;
    tSetWidtploy<<(SetPos+QPointF(s*Height,c*Height));
    tSetWidtploy<<(SetPos-QPointF(s*Height,c*Height));
    tSetWidtploy<<(SetPos-QPointF(s*Height,c*Height)-seti*QPointF(m_setScal*10*cos(atn1-0.5+MV_PI_2),m_setScal*10*sin(atn1+MV_PI_2-0.5)));
    tSetWidtploy<<(SetPos-QPointF(s*Height,c*Height));
    tSetWidtploy<<(SetPos-QPointF(s*Height,c*Height)+seti*QPointF(m_setScal*10*cos(atn1-MV_PI_2+0.5),m_setScal*10*sin(atn1-MV_PI_2+0.5)));
    m_ItemPath.addPolygon(tSetWidtploy);
    SetPos=(polygon[1]+Center)/2;
    tSetWidtploy.clear();
    tSetWidtploy<<(SetPos+QPointF(s*Height,c*Height));
    tSetWidtploy<<(SetPos-QPointF(s*Height,c*Height));
    tSetWidtploy<<(SetPos-QPointF(s*Height,c*Height)-seti*QPointF(m_setScal*10*cos(atn1-0.5+MV_PI_2),m_setScal*10*sin(atn1+MV_PI_2-0.5)));
    tSetWidtploy<<(SetPos-QPointF(s*Height,c*Height));
    tSetWidtploy<<(SetPos-QPointF(s*Height,c*Height)+seti*QPointF(m_setScal*10*cos(atn1-MV_PI_2+0.5),m_setScal*10*sin(atn1-MV_PI_2+0.5)));
    m_ItemPath.addPolygon(tSetWidtploy);

    //
    QPainterPath PathVir;

    QTransform trans;
    trans.translate(Center.x(),Center.y());
    trans.rotate(angle*180/M_PI);
    //旋转到水平、画椭圆只能水平状态 angle
    PathVir.addRect(-Lenth/2,-abs(Height),Lenth,abs(Height*2));
    //旋转到正常角度
    trans.rotate(-2*angle*180/M_PI);
    //将画好的椭圆旋转到正常位置
    PathVir=trans.map(PathVir);
    m_ItemPath.addPath(PathVir);


    return true;
}

void QtLineROI::toGetItemVal()
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

QVector<double> QtLineROI::toGetDatavalue()
{
    return m_CMvLineSeg.Data();
}
