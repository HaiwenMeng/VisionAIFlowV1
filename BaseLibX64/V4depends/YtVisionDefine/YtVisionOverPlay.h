#ifndef YTVISIONOVERPLAY_H
#define YTVISIONOVERPLAY_H
#include "ytvisiondefine.h"
#include <QFont>
#include <QPainter>
#include <QTransform>
#include <QDebug>
#include <QMutex>
//设定一个图层结构，只记录常规数据结构，
//一个图层就一个颜色其实不是很合适，管理好一个图层集合里面的数据很重要
struct YtGetRestObj
{
    //基础结果定义
    QVector<double>         ResultDoble;//结果浮点数
    QVector<int>            Resultint;//结果整形
    QVector<bool>           ResultBool;//结果浮点型
    QVector<QString>        ResultQString;//qstring
    QVector<CMvPoint>       ResultPointslist;//显示点集，固定大小的x
    QVector<CMvLine>        ResultLinelist;//直线
    QVector<CMvLineSeg>     ResultLineSeglist;//线段
    QVector<CMvRect>        ResultRectlist;//正矩形
    QVector<CMvRotatedRect> ResultRotateRectlist;//旋转矩形
    QVector<CMvPolygon>     Polygonlist;//循环点集
    QVector<QVector<CMvPoint>> PolygonElist;//非循环点集
    QVector<CMvCircle>         CircleList;//圆列表
    QVector<CMvEllipse>        EllipseList;//椭圆列表
    QVector<CMvCoord>          MvCoordList;//坐标系列表
    QVector<CMvArc>            MvArcList;//圆弧列表
    QVector<CMvPoint3d>        MvPoint3dList;//三维点列表
    QVector<CMvWaistRound>     MvWaistRoundList;//腰圆列表
    void toClearData()
    {
        ResultDoble.clear();
        Resultint.clear();//结果整形
        ResultBool.clear();//结果浮点型
        ResultQString.clear();//qs
        ResultPointslist.clear();//
        ResultLinelist.clear();//直
        ResultLineSeglist.clear();
        ResultRectlist.clear();//正
        ResultRotateRectlist.clear();
        Polygonlist.clear();//循环点集
        PolygonElist.clear();//非循环点集
        CircleList.clear();//圆列表
        EllipseList.clear();//椭圆列表
        MvCoordList.clear();//坐标系列表
        MvArcList.clear();//圆弧列表
        MvPoint3dList.clear();//三维点列表
        MvWaistRoundList.clear();//腰圆列表

    }
    int ResultCount()
    {
        int tNumber=0;
        tNumber+=ResultDoble.size();
        tNumber+=Resultint.size();//结果整形
        tNumber+=ResultBool.size();//结果浮点型
        tNumber+=ResultQString.size();//qs
        tNumber+=ResultPointslist.size();//
        tNumber+=ResultLinelist.size();//直
        tNumber+=ResultLineSeglist.size();
        tNumber+=ResultRectlist.size();//正
        tNumber+=ResultRotateRectlist.size();
        tNumber+=Polygonlist.size();//循环点集
        tNumber+=PolygonElist.size();//非循环点集
        tNumber+=CircleList.size();//圆列表
        tNumber+=EllipseList.size();//椭圆列表
        tNumber+=MvCoordList.size();//坐标系列表
        tNumber+=MvArcList.size();//圆弧列表
        tNumber+=MvPoint3dList.size();//三维点列表
        tNumber+=MvWaistRoundList.size();//腰圆列表
        return tNumber;
    }
    void AddByAnother(YtGetRestObj *Obj)
    {
        ResultDoble.append(Obj->ResultDoble);
        Resultint.append(Obj->Resultint);//结果整形
        ResultBool.append(Obj->ResultBool);//结果浮点型
        ResultQString.append(Obj->ResultQString);//qs
        ResultPointslist.append(Obj->ResultPointslist);//
        ResultLinelist.append(Obj->ResultLinelist);//直
        ResultLineSeglist.append(Obj->ResultLineSeglist);
        ResultRectlist.append(Obj->ResultRectlist);//正
        ResultRotateRectlist.append(Obj->ResultRotateRectlist);
        Polygonlist.append(Obj->Polygonlist);//循环点集
        PolygonElist.append(Obj->PolygonElist);//非循环点集
        CircleList.append(Obj->CircleList);//圆列表
        EllipseList.append(Obj->EllipseList);//椭圆列表
        MvCoordList.append(Obj->MvCoordList);//坐标系列表
        MvArcList.append(Obj->MvArcList);//圆弧列表
        MvPoint3dList.append(Obj->MvPoint3dList);//三维点列表
        MvWaistRoundList.append(Obj->MvWaistRoundList);//腰圆列表
    }
};
struct LabTxt
{
    //有背景字体
    QString showText;//显示内容
    CMvPoint Position;//显示位置
    int LabSize=0;//背景色字符长度设定，0位自动跟着字走
    QColor clrTxt;//字的颜色
    QColor clrBg;//背景颜色
    QFont FtTxt;//字体
    LabTxt()
    {

    }
    LabTxt(QString Settxt,CMvPoint tpos=CMvPoint(0,0),QFont yFtTxt=QFont("Times",13,14),QColor txtclr=Qt::red,int SetSize=0,QColor bgclr=Qt::transparent)
    {
        showText=Settxt;
        Position=tpos;
        LabSize=SetSize;
        clrTxt=txtclr;
        clrBg=bgclr;
        FtTxt=yFtTxt;
    }
    void toPaintStd(QPainter *tpanter,QRectF SetRect)
    {
        QRectF tBackSize;
        QPen tpen;
        QFont tFornt=FtTxt;
        tFornt.setPointSizeF(FtTxt.pointSizeF());
        tpen.setColor(clrTxt);
        tpanter->setFont(tFornt);
        tpanter->setPen(tpen);
        if(LabSize<1)
        {
            tBackSize=tpanter->fontMetrics().boundingRect(showText);
        }
        else
        {
            QString temst;
            temst=temst.fill('C',LabSize);
            tBackSize=tpanter->fontMetrics().boundingRect(showText);
        }
        CMvPoint SPosition=Position;
        if(Position.x<0)
        {
            SPosition.x=SetRect.width()-tBackSize.width()+Position.x;
        }
        if(Position.y<0)
        {
            SPosition.y=SetRect.height()-tBackSize.height()+Position.y;
        }
        tpanter->fillRect(QRect(QPoint(SPosition.x,SPosition.y),QSize(tBackSize.width(),tBackSize.height())),clrBg);

        tpanter->drawText(QPoint(SPosition.x,SPosition.y+tFornt.pointSize()),showText);
    }
    void toPaint(QPainter *tpanter,QRectF SetRect,qreal SetScal)
    {
        QRectF tBackSize;
        QPen tpen;
        QFont tFornt=FtTxt;
        tFornt.setPointSizeF(YtMAX(2.0,FtTxt.pointSizeF()/SetScal));
        tpen.setColor(clrTxt);
        tpanter->setFont(tFornt);
        tpanter->setPen(tpen);
        if(LabSize<1)
        {
            tBackSize=tpanter->fontMetrics().boundingRect(showText);
        }
        else
        {
            QString temst;
            temst=temst.fill('C',LabSize);
            tBackSize=tpanter->fontMetrics().boundingRect(showText);
        }
        CMvPoint SPosition=Position;
        if(Position.x<0)
        {
            SPosition.x=SetRect.width()-Position.x-tBackSize.width();
        }
        if(Position.y<0)
        {
            SPosition.y=SetRect.height()-Position.y-tBackSize.height();
        }
        tpanter->fillRect(QRect(QPoint(SPosition.x,SPosition.y),QSize(tBackSize.width(),tBackSize.height())),clrBg);

        tpanter->drawText(QPoint(SPosition.x,SPosition.y+tFornt.pointSize()),showText);
    }
};
struct DispTxt
{
    //无背景字体
    QString showText;//显示内容
    CMvPoint Position;//显示位置
    QColor clrTxt;//字的颜色
    QFont FtTxt;//字体
    DispTxt()
    {

    }
    DispTxt(QString Settxt,CMvPoint tpos=CMvPoint(0,0),QFont yFtTxt=QFont("Times",13,14),QColor txtclr=Qt::red)
    {
        showText=Settxt;
        Position=tpos;
        clrTxt=txtclr;
        FtTxt=yFtTxt;
    }
    void toPaintStd(QPainter *tpanter,QRectF SetRect)
    {
        QPen tpen;
        QFont tFornt=FtTxt;
        tFornt.setPointSizeF(FtTxt.pointSizeF());
        tpen.setColor(clrTxt);
        tpanter->setFont(tFornt);
        tpanter->setPen(tpen);
        CMvPoint SPosition=Position;

        if(Position.x<0)
        {
            QRectF tBackSize=tpanter->boundingRect(SetRect,showText);
            SPosition.x=SetRect.width()-tBackSize.width()+Position.x;
        }
        if(Position.y<0)
        {
            QRectF tBackSize=tpanter->boundingRect(SetRect,showText);
            SPosition.y=SetRect.height()-tBackSize.height()+Position.y;
        }
        tpanter->drawText(QPoint(SPosition.x,SPosition.y+tFornt.pointSize()),showText);
    }
    void toPaint(QPainter *tpanter,qreal SetScal)
    {
        QPen tpen;
        QFont tFornt=FtTxt;
        tFornt.setPointSizeF(YtMAX(1.0,FtTxt.pointSizeF()/SetScal));
        tpen.setColor(clrTxt);
        tpanter->setFont(tFornt);
        tpanter->setPen(tpen);
        CMvPoint SPosition=Position;
        if(Position.x<0)
        {
            SPosition.x=0;
        }
        if(Position.y<0)
        {
            SPosition.y=0;
        }
        tpanter->drawText(QPoint(SPosition.x,SPosition.y+tFornt.pointSize()),showText);
    }
};
//带箭头的直线绘制
struct DispLineSegArow
{
    CMvLineSeg GetLine;
    QString ShowTxt;//长度信息或者什么信息
    QColor clrTxt;//字的颜色，背景色直接反色拉倒
    QFont FtTxt;//字体
    int SetLineWidth;//绘制线宽
    DispLineSegArow()
    {

    }
    DispLineSegArow(CMvLineSeg tGetLine,QString tShowTxt,int tSetLineWidth=1,QColor tclrTxt=Qt::red,QFont tFtTxt=QFont("Times",13,14))
    {
        GetLine=tGetLine;
        ShowTxt=tShowTxt;
        clrTxt=tclrTxt;
        FtTxt=tFtTxt;
        SetLineWidth=tSetLineWidth;
    }
    void toPaintStd(QPainter *tpanter)
    {
        QPen tpen;
        QFont tFornt=FtTxt;
        tFornt.setPointSizeF(FtTxt.pointSizeF());
        tpen.setColor(Qt::white);
        tpen.setWidthF(SetLineWidth);
        tpanter->setFont(tFornt);
        tpanter->setPen(tpen);
        tpanter->drawLine(QLineF(GetLine.st.x,GetLine.st.y,GetLine.ed.x,GetLine.ed.y));
        double tGetAng=GetLine.Rad();
        tpanter->drawLine(QPointF(GetLine.st.x,GetLine.st.y),QPointF(GetLine.st.x+10*cos(tGetAng-0.5),GetLine.st.y+10*sin(tGetAng-0.5)));
        tpanter->drawLine(QPointF(GetLine.st.x,GetLine.st.y),QPointF(GetLine.st.x+10*cos(tGetAng+0.5),GetLine.st.y+10*sin(tGetAng+0.5)));

        tpanter->drawLine(QPointF(GetLine.ed.x,GetLine.ed.y),QPointF(GetLine.ed.x-10*cos(tGetAng-0.5),GetLine.ed.y-10*sin(tGetAng-0.5)));
        tpanter->drawLine(QPointF(GetLine.ed.x,GetLine.ed.y),QPointF(GetLine.ed.x-10*cos(tGetAng+0.5),GetLine.ed.y-10*sin(tGetAng+0.5)));
        QRectF tBackSize=tpanter->boundingRect(QRectF(0,0,1000,50),ShowTxt);
        tpanter->fillRect(QRect(QPoint(GetLine.CenterX(),GetLine.CenterY()),QSize(tBackSize.width(),tBackSize.height())),
                         clrTxt);
        tpanter->drawText(QPoint(GetLine.CenterX(),GetLine.CenterY()+tFornt.pointSize()),ShowTxt);
    }
    void toPaint(QPainter *tpanter,qreal SetScal)
    {
        QPen tpen;
        QFont tFornt=FtTxt;
        tFornt.setPointSizeF(YtMAX(2.0,FtTxt.pointSizeF()/SetScal));
        tpen.setColor(clrTxt);
        tpen.setWidthF(YtMAX(1,SetLineWidth));
        tpanter->setFont(tFornt);
        tpanter->setPen(tpen);
        tpanter->drawLine(QLineF(GetLine.st.x,GetLine.st.y,GetLine.ed.x,GetLine.ed.y));
        double tGetAng=GetLine.Rad();
        tpanter->drawLine(QPointF(GetLine.st.x,GetLine.st.y),QPointF(GetLine.st.x+10*cos(tGetAng-0.5),GetLine.st.y+10*sin(tGetAng-0.5)));
        tpanter->drawLine(QPointF(GetLine.st.x,GetLine.st.y),QPointF(GetLine.st.x+10*cos(tGetAng+0.5),GetLine.st.y+10*sin(tGetAng+0.5)));

        tpanter->drawLine(QPointF(GetLine.ed.x,GetLine.ed.y),QPointF(GetLine.ed.x-10*cos(tGetAng-0.5),GetLine.ed.y-10*sin(tGetAng-0.5)));
        tpanter->drawLine(QPointF(GetLine.ed.x,GetLine.ed.y),QPointF(GetLine.ed.x-10*cos(tGetAng+0.5),GetLine.ed.y-10*sin(tGetAng+0.5)));

        QRectF tBackSize=tpanter->boundingRect(QRectF(0,0,1000,1000),ShowTxt);
        tpanter->fillRect(QRect(QPoint(GetLine.CenterX(),GetLine.CenterY()),QSize(tBackSize.width(),tBackSize.height())),
                          QColor(255-clrTxt.red(),255-clrTxt.green(),255-clrTxt.blue()));

        tpanter->drawText(QPoint(GetLine.CenterX(),GetLine.CenterY()+tFornt.pointSize()),ShowTxt);

    }

};

//显示点集，点的显示长度伴随像素固定显示即可
struct DispPointS
{
    QVector<CMvPoint> ShowPos;//显示点集
    int AngSet=90;//点显示的角度
    int SetLen=3;//点的长度
    QColor clrPoint=Qt::red;//颜色
    DispPointS()
    {

    }
    DispPointS(CMvPoint SetPos,QColor tclrPoint=Qt::red,int tSetLen=3,int tAngSet=90)
    {
        ShowPos.append(SetPos);
        AngSet=tAngSet;
        SetLen=tSetLen;
        clrPoint=tclrPoint;
    }
    DispPointS(QVector<CMvPoint> &SetPos,QColor tclrPoint=Qt::red,int tSetLen=3,int tAngSet=90)
    {
        ShowPos.append(SetPos);
        AngSet=tAngSet;
        SetLen=tSetLen;
        clrPoint=tclrPoint;
    }
    void toApendPoint(CMvPoint SetPos)
    {
        ShowPos.append(SetPos);
    }
    void toApendPoint(QVector<CMvPoint> &SetPos)
    {
        ShowPos.append(SetPos);
    }
    void toClearPoint()
    {
        ShowPos.clear();
    }
    void toSetShow(QColor tSetCor,int Ang=90,int lenset=3)
    {
        clrPoint=tSetCor;
        AngSet=Ang;
        SetLen=lenset;
    }
    void toPaintStd(QPainter *tpanter)
    {
        QPen tpen;
        tpen.setColor(clrPoint);
        tpen.setWidthF(1);
        tpanter->setPen(tpen);
        double tGetAng=1.0*AngSet/180*MV_PI;
        for(int i=0;i<ShowPos.size();i++)
        {
            tpanter->drawLine( QPointF(ShowPos[i].x-SetLen*cos(tGetAng),ShowPos[i].y-SetLen*sin(tGetAng)),
                               QPointF(ShowPos[i].x+SetLen*cos(tGetAng),ShowPos[i].y+SetLen*sin(tGetAng)));
            tpanter->drawLine( QPointF(ShowPos[i].x-SetLen*cos(MV_PI_2+tGetAng),ShowPos[i].y-SetLen*sin(MV_PI_2+tGetAng)),
                               QPointF(ShowPos[i].x+SetLen*cos(MV_PI_2+tGetAng),ShowPos[i].y+SetLen*sin(MV_PI_2+tGetAng)));
        }
    }
    void toPaint(QPainter *tpanter,qreal SetScal)
    {
        QPen tpen;
        tpen.setColor(clrPoint);
        tpen.setWidthF(1.0/SetScal);
        tpanter->setPen(tpen);
        double tGetAng=1.0*AngSet/180*MV_PI;
        double DrawLen=YtMAX(1.0,SetLen/SetScal);
        for(int i=0;i<ShowPos.size();i++)
        {
            tpanter->drawLine( QPointF(ShowPos[i].x-DrawLen*cos(tGetAng),ShowPos[i].y-DrawLen*sin(tGetAng)),
                               QPointF(ShowPos[i].x+DrawLen*cos(tGetAng),ShowPos[i].y+DrawLen*sin(tGetAng)));
            tpanter->drawLine( QPointF(ShowPos[i].x-DrawLen*cos(MV_PI_2+tGetAng),ShowPos[i].y-DrawLen*sin(MV_PI_2+tGetAng)),
                               QPointF(ShowPos[i].x+DrawLen*cos(MV_PI_2+tGetAng),ShowPos[i].y+DrawLen*sin(MV_PI_2+tGetAng)));
        }

    }
};
//显示线段
struct  DispLineSegs
{
    QVector<CMvLineSeg> ShowLineSeg;//显示线段集
    int SetWidth=3;//线宽度
    QColor clrLine=Qt::red;//颜色
    bool IsDotoLine=false;
    DispLineSegs()
    {

    }
    DispLineSegs(CMvLineSeg SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowLineSeg.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    DispLineSegs(QVector<CMvLineSeg> &SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowLineSeg.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    void toApendLineSeg(CMvLineSeg SetSeg)
    {
        ShowLineSeg.append(SetSeg);
    }
    void toApendLineSeg(QVector<CMvLineSeg> &SetSeg)
    {
        ShowLineSeg.append(SetSeg);
    }
    void toClearData()
    {
        ShowLineSeg.clear();
    }
    void toSetShow(QColor tSetCor,int lenWidth=3,bool isdoto=false)
    {
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    void toPaintStd(QPainter *tpanter)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowLineSeg.size();i++)
        {
            tpanter->drawLine( QPointF(ShowLineSeg[i].st.x,ShowLineSeg[i].st.y),
                               QPointF(ShowLineSeg[i].ed.x,ShowLineSeg[i].ed.y));
        }
    }
    void toPaint(QPainter *tpanter,qreal SetScal)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth/SetScal);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowLineSeg.size();i++)
        {
            tpanter->drawLine( QPointF(ShowLineSeg[i].st.x,ShowLineSeg[i].st.y),
                               QPointF(ShowLineSeg[i].ed.x,ShowLineSeg[i].ed.y));
        }

    }
};
//显示线段
struct  DispLines
{
    QVector<CMvLine> ShowLines;//显示线段集
    int SetWidth=3;//线宽度
    QColor clrLine=Qt::red;//颜色
    bool IsDotoLine=false;
    DispLines()
    {

    }
    DispLines(CMvLine SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowLines.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    DispLines(QVector<CMvLine> &SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowLines.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    void toApendLineSeg(CMvLine SetSeg)
    {
        ShowLines.append(SetSeg);
    }
    void toApendLineSeg(QVector<CMvLine> &SetSeg)
    {
        ShowLines.append(SetSeg);
    }
    void toClearData()
    {
        ShowLines.clear();
    }
    void toSetShow(QColor tSetCor,int lenWidth=3,bool isdoto=false)
    {
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    void toPaintStd(QPainter *tpanter)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);

        for(int i=0;i<ShowLines.size();i++)
        {
            double dx=99999*cos(ShowLines[i].angle*M_PI_180);
            double dy=99999*sin(ShowLines[i].angle*M_PI_180);
            tpanter->drawLine(ShowLines[i].CenterP.x-dx,ShowLines[i].CenterP.y-dy,
                              ShowLines[i].CenterP.x+dx,ShowLines[i].CenterP.y+dy);
        }
    }
    void toPaint(QPainter *tpanter,qreal SetScal)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth/SetScal);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowLines.size();i++)
        {
            double dx=99999*cos(ShowLines[i].angle*M_PI_180);
            double dy=99999*sin(ShowLines[i].angle*M_PI_180);
            tpanter->drawLine(ShowLines[i].CenterP.x-dx,ShowLines[i].CenterP.y-dy,
                              ShowLines[i].CenterP.x+dx,ShowLines[i].CenterP.y+dy);
        }

    }
};


//显示正矩形框
struct  DispRects
{
    QVector<CMvRect> ShowDispRects;//显示正矩形
    int SetWidth=3;//线宽度
    QColor clrLine=Qt::red;//颜色
    bool IsFill=false;//是否填充绘制
    bool IsDotoLine=false;
    DispRects()
    {

    }
    DispRects(CMvRect SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool setfill=false,bool isdoto=false)
    {
        ShowDispRects.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsFill=setfill;
        IsDotoLine=isdoto;
    }
    DispRects(QVector<CMvRect> &SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool setfill=false,bool isdoto=false)
    {
        ShowDispRects.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsFill=setfill;
        IsDotoLine=isdoto;
    }
    void toApendLineSeg(CMvRect SetSeg)
    {
        ShowDispRects.append(SetSeg);
    }
    void toApendLineSeg(QVector<CMvRect> &SetSeg)
    {
        ShowDispRects.append(SetSeg);
    }
    void toClearData()
    {
        ShowDispRects.clear();
    }
    void toSetShow(QColor tSetCor,int lenWidth=3,bool setfill=false,bool isdoto=false)
    {
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsFill=setfill;
        IsDotoLine=isdoto;
    }
    void toPaintStd(QPainter *tpanter)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowDispRects.size();i++)
        {
            if(IsFill)
            {
                tpanter->fillRect(ShowDispRects[i].LeftTop.x,ShowDispRects[i].LeftTop.y,ShowDispRects[i].cx,ShowDispRects[i].cy,clrLine);
            }
            else
            {
                tpanter->drawRect(ShowDispRects[i].LeftTop.x,ShowDispRects[i].LeftTop.y,ShowDispRects[i].cx,ShowDispRects[i].cy);
            }
        }
    }
    void toPaint(QPainter *tpanter,qreal SetScal)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth/SetScal);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowDispRects.size();i++)
        {
            if(IsFill)
            {
                tpanter->fillRect(ShowDispRects[i].LeftTop.x,ShowDispRects[i].LeftTop.y,ShowDispRects[i].cx,ShowDispRects[i].cy,clrLine);
            }
            else
            {
                tpanter->drawRect(ShowDispRects[i].LeftTop.x,ShowDispRects[i].LeftTop.y,ShowDispRects[i].cx,ShowDispRects[i].cy);
            }
        }

    }
};

//显示正圆
struct  DispCircles
{
    QVector<CMvCircle> ShowCircles;//显示正圆
    int SetWidth=3;//线宽度
    QColor clrLine=Qt::red;//颜色
    bool IsDotoLine=false;
    DispCircles()
    {

    }
    DispCircles(CMvCircle SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowCircles.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    DispCircles(QVector<CMvCircle> &SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowCircles.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    void toApendLineSeg(CMvCircle SetSeg)
    {
        ShowCircles.append(SetSeg);
    }
    void toApendLineSeg(QVector<CMvCircle> &SetSeg)
    {
        ShowCircles.append(SetSeg);
    }
    void toClearData()
    {
        ShowCircles.clear();
    }
    void toSetShow(QColor tSetCor,int lenWidth=3,bool isdoto=false)
    {
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    void toPaintStd(QPainter *tpanter)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowCircles.size();i++)
        {
            tpanter->drawEllipse(ShowCircles[i].center.x-ShowCircles[i].radius,
                                 ShowCircles[i].center.y-ShowCircles[i].radius,
                                 2*ShowCircles[i].radius,2*ShowCircles[i].radius);

        }
    }
    void toPaint(QPainter *tpanter,qreal SetScal)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth/SetScal);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowCircles.size();i++)
        {
            tpanter->drawEllipse(ShowCircles[i].center.x-ShowCircles[i].radius,
                                 ShowCircles[i].center.y-ShowCircles[i].radius,
                                 2*ShowCircles[i].radius,2*ShowCircles[i].radius);

        }
    }
};
//显示椭圆
struct  DispEllipses
{
    QVector<CMvEllipse> ShowEllipses;//显示正圆
    int SetWidth=3;//线宽度
    QColor clrLine=Qt::red;//颜色
    bool IsDotoLine=false;
    DispEllipses()
    {

    }
    DispEllipses(CMvEllipse SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowEllipses.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;

    }
    DispEllipses(QVector<CMvEllipse> &SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowEllipses.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;

    }
    void toApendLineSeg(CMvEllipse SetSeg)
    {
        ShowEllipses.append(SetSeg);
    }
    void toApendLineSeg(QVector<CMvEllipse> &SetSeg)
    {
        ShowEllipses.append(SetSeg);
    }
    void toClearData()
    {
        ShowEllipses.clear();
    }
    void toSetShow(QColor tSetCor,int lenWidth=3,bool isdoto=false)
    {
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    void toPaintStd(QPainter *tpanter)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowEllipses.size();i++)
        {
            tpanter->save();
            tpanter->translate(ShowEllipses[i].Center.x,ShowEllipses[i].Center.y);
            tpanter->rotate(-ShowEllipses[i].Deg());
            tpanter->drawEllipse(QRectF({-ShowEllipses[i].axisX, -ShowEllipses[i].axisY}, QPointF(ShowEllipses[i].axisX, ShowEllipses[i].axisY)));
            tpanter->restore();

        }
    }
    void toPaint(QPainter *tpanter,qreal SetScal)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth/SetScal);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowEllipses.size();i++)
        {

            tpanter->save();
            tpanter->translate(ShowEllipses[i].Center.x,ShowEllipses[i].Center.y);
            tpanter->rotate(-ShowEllipses[i].Deg());
            tpanter->drawEllipse(QRectF({-ShowEllipses[i].axisX, -ShowEllipses[i].axisY}, QPointF(ShowEllipses[i].axisX, ShowEllipses[i].axisY)));
            tpanter->restore();
        }
    }
};
//显示斜矩形
struct  DispRotatedRects
{
    QVector<CMvRotatedRect> ShowRotatedRects;//显示正圆
    int SetWidth=3;//线宽度
    QColor clrLine=Qt::red;//颜色
    bool IsDotoLine=false;
    DispRotatedRects()
    {

    }
    DispRotatedRects(CMvRotatedRect SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowRotatedRects.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    DispRotatedRects(QVector<CMvRotatedRect> &SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowRotatedRects.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    void toApendRotatedRects(CMvRotatedRect SetSeg)
    {
        ShowRotatedRects.append(SetSeg);
    }
    void toApendRotatedRects(QVector<CMvRotatedRect> &SetSeg)
    {
        ShowRotatedRects.append(SetSeg);
    }
    void toClearData()
    {
        ShowRotatedRects.clear();
    }
    void toSetShow(QColor tSetCor,int lenWidth=3,bool isdoto=false)
    {
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    void toPaintStd(QPainter *tpanter)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowRotatedRects.size();i++)
        {
            tpanter->save();
            tpanter->translate(ShowRotatedRects[i].Center.x,ShowRotatedRects[i].Center.y);
            tpanter->rotate(-ShowRotatedRects[i].Deg());
            tpanter->drawRect(QRectF({-ShowRotatedRects[i].cx, -ShowRotatedRects[i].cy}, QPointF(ShowRotatedRects[i].cx, ShowRotatedRects[i].cy)));
            tpanter->restore();

        }
    }
    void toPaint(QPainter *tpanter,qreal SetScal)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth/SetScal);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowRotatedRects.size();i++)
        {
            tpanter->save();
            tpanter->translate(ShowRotatedRects[i].Center.x,ShowRotatedRects[i].Center.y);
            tpanter->rotate(-ShowRotatedRects[i].Deg());
            tpanter->drawRect(QRectF({-ShowRotatedRects[i].cx, -ShowRotatedRects[i].cy}, QPointF(ShowRotatedRects[i].cx, ShowRotatedRects[i].cy)));
            tpanter->restore();
        }
    }
};
//显示折线
struct  DispPolygons
{
    QVector<CMvPolygon> ShowPolygons;//显示循环点集
    int SetWidth=3;//线宽度
    QColor clrLine=Qt::red;//颜色
    bool IsDotoLine=false;
    DispPolygons()
    {

    }
    DispPolygons(CMvPolygon SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowPolygons.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;

    }
    DispPolygons(QVector<CMvPolygon> &SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowPolygons.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;

    }
    void toApendLineSeg(CMvPolygon SetSeg)
    {
        ShowPolygons.append(SetSeg);
    }
    void toApendLineSeg(QVector<CMvPolygon> &SetSeg)
    {
        ShowPolygons.append(SetSeg);
    }
    void toClearData()
    {
        ShowPolygons.clear();
    }
    void toSetShow(QColor tSetCor,int lenWidth=3,bool isdoto=false)
    {
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    void toPaintStd(QPainter *tpanter)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowPolygons.size();i++)
        {
            int tGetPointsize=ShowPolygons[i].points.size();
            if(tGetPointsize<2)
            {
                continue;
            }
            for(int j=0;j<tGetPointsize-1;j++)
            {
                tpanter->drawLine(ShowPolygons[i].points[j].x,
                                  ShowPolygons[i].points[j].y,
                                  ShowPolygons[i].points[j+1].x,
                        ShowPolygons[i].points[j+1].y);
            }
            tpanter->drawLine(ShowPolygons[i].points[0].x,
                    ShowPolygons[i].points[0].y,
                    ShowPolygons[i].points[tGetPointsize-1].x,
                    ShowPolygons[i].points[tGetPointsize-1].y);


        }
    }
    void toPaint(QPainter *tpanter,qreal SetScal)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth/SetScal);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowPolygons.size();i++)
        {
            int tGetPointsize=ShowPolygons[i].points.size();
            if(tGetPointsize<2)
            {
                continue;
            }
            for(int j=0;j<tGetPointsize-1;j++)
            {
                tpanter->drawLine(ShowPolygons[i].points[j].x,
                                  ShowPolygons[i].points[j].y,
                                  ShowPolygons[i].points[j+1].x,
                        ShowPolygons[i].points[j+1].y);
            }
            tpanter->drawLine(ShowPolygons[i].points[0].x,
                    ShowPolygons[i].points[0].y,
                    ShowPolygons[i].points[tGetPointsize-1].x,
                    ShowPolygons[i].points[tGetPointsize-1].y);


        }
    }
};

//显示非循环点集
struct  DispPolygonEs
{
    QVector<CMvPolygon> ShowPolygons;//显示循环点集
    int SetWidth=3;//线宽度
    QColor clrLine=Qt::red;//颜色
    bool IsDotoLine=false;
    DispPolygonEs()
    {

    }
    DispPolygonEs(CMvPolygon SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowPolygons.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;

    }
    DispPolygonEs(QVector<CMvPolygon> &SetSeg,QColor tSetCor=Qt::red,int lenWidth=3,bool isdoto=false)
    {
        ShowPolygons.append(SetSeg);
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;

    }
    void toApendLineSeg(CMvPolygon SetSeg)
    {
        ShowPolygons.append(SetSeg);
    }
    void toApendLineSeg(QVector<CMvPolygon> &SetSeg)
    {
        ShowPolygons.append(SetSeg);
    }
    void toClearData()
    {
        ShowPolygons.clear();
    }
    void toSetShow(QColor tSetCor,int lenWidth=3,bool isdoto=false)
    {
        clrLine=tSetCor;
        SetWidth=lenWidth;
        IsDotoLine=isdoto;
    }
    void toPaintStd(QPainter *tpanter)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowPolygons.size();i++)
        {
            int tGetPointsize=ShowPolygons[i].points.size();
            if(tGetPointsize<2)
            {
                continue;
            }
            for(int j=0;j<tGetPointsize-1;j++)
            {
                tpanter->drawLine(ShowPolygons[i].points[j].x,
                                  ShowPolygons[i].points[j].y,
                                  ShowPolygons[i].points[j+1].x,
                        ShowPolygons[i].points[j+1].y);
            }
        }
    }
    void toPaint(QPainter *tpanter,qreal SetScal)
    {
        QPen tpen;
        tpen.setColor(clrLine);
        tpen.setWidthF(SetWidth/SetScal);
        if(IsDotoLine)
        {
            tpen.setStyle(Qt::DashLine);
        }
        tpanter->setPen(tpen);
        for(int i=0;i<ShowPolygons.size();i++)
        {
            int tGetPointsize=ShowPolygons[i].points.size();
            if(tGetPointsize<2)
            {
                continue;
            }
            for(int j=0;j<tGetPointsize-1;j++)
            {
                tpanter->drawLine(ShowPolygons[i].points[j].x,
                                  ShowPolygons[i].points[j].y,
                                  ShowPolygons[i].points[j+1].x,
                        ShowPolygons[i].points[j+1].y);
            }
        }
    }
};
//接口已void * 进去内部根据自己的显示定义去设定
//设计显示图层标准定义，每个显示基元都有自己颜色以及类型定义，我们根据需要显示的完成显示的基本定义
//首先都是构造结果变量 后续再构造出显示需要的其他变量即可。主要是能够控制不同的颜色针对同一图层
struct YtSetShowtObj
{
    QVector<LabTxt> m_LabTxt;//带背景的字显示
    QVector<DispTxt> m_DispTxt;//不带背景的字显示
    QVector<DispLineSegArow> m_DispLineSegArow;//包含双头箭头的显示
    QVector<DispPointS> m_DispPointS;//用x显示点
    QVector<DispLineSegs> m_DispLineSegs;//用显示线段
    QVector<DispLines> m_DispLines;//显示直线
    QVector<DispRects> m_DispRects;//显示正矩形
    QVector<DispCircles> m_DispCircles;//显示正圆
    QVector<DispEllipses> m_DispEllipses;//显示椭圆
    QVector<DispRotatedRects> m_DispRotatedRects;//显示斜矩形
    QVector<DispPolygons> m_DispPolygons;//显示循环点集
    QVector<DispPolygonEs> m_DispPolygonEs;//显示非循环点集
    QImage m_MaskIm=QImage();//图层图像
    QMutex m_mutex;
    ////////
    void toClearData()
    {
        QMutexLocker locker(&m_mutex);
        m_LabTxt.clear();
        m_DispTxt.clear();
        m_DispLineSegArow.clear();
        m_DispPointS.clear();
        m_DispLineSegs.clear();
        m_DispLines.clear();
        m_DispRects.clear();
        m_DispCircles.clear();
        m_DispEllipses.clear();
        m_DispRotatedRects.clear();
        m_DispPolygons.clear();
        m_DispPolygonEs.clear();
    }
    void toClearMask()
    {
        QMutexLocker locker(&m_mutex);
        m_MaskIm=QImage();
    }
    void toSetMaskIm(QImage SetImage,QVector<QRgb> sColorTable=QVector<QRgb>())
    {
        QMutexLocker locker(&m_mutex);
        if(sColorTable.size() < 1)
        {
            sColorTable.push_back(qRgba(255, 0, 0, 128));
        }
        int tcolorsize = sColorTable.size();
        if(SetImage.format()==QImage::Format_Indexed8)
        {
            m_MaskIm=SetImage;
        }
        else
        {
            if(m_MaskIm.size()!=SetImage.size())
            {
                m_MaskIm=QImage(SetImage.size(),QImage::Format_Indexed8);
            }
            memcpy(m_MaskIm.bits(),SetImage.bits(),m_MaskIm.sizeInBytes());
            m_MaskIm.setColor(0, qRgba(0, 0, 0, 0));
            for(int i = 1; i < 256; i++)
            {
                m_MaskIm.setColor(i, sColorTable.at(i % tcolorsize));
            }
        }
    }
    void toPaintStd(QPainter *tpanter,QRect SetRect)
    {
        QMutexLocker locker(&m_mutex);
        for(int i=0;i<m_LabTxt.size();i++)
        {

            m_LabTxt[i].toPaintStd(tpanter,SetRect);
        }
        for(int i=0;i<m_DispTxt.size();i++)
        {
            m_DispTxt[i].toPaintStd(tpanter,SetRect);
        }
        for(int i=0;i<m_DispLineSegArow.size();i++)
        {
            m_DispLineSegArow[i].toPaintStd(tpanter);
        }
        for(int i=0;i<m_DispPointS.size();i++)
        {
            m_DispPointS[i].toPaintStd(tpanter);
        }
        for(int i=0;i<m_DispLineSegs.size();i++)
        {
            m_DispLineSegs[i].toPaintStd(tpanter);
        }
        for(int i=0;i<m_DispLines.size();i++)
        {
            m_DispLines[i].toPaintStd(tpanter);
        }
        for(int i=0;i<m_DispRects.size();i++)
        {
            m_DispRects[i].toPaintStd(tpanter);
        }
        for(int i=0;i<m_DispCircles.size();i++)
        {
            m_DispCircles[i].toPaintStd(tpanter);
        }
        for(int i=0;i<m_DispEllipses.size();i++)
        {
            m_DispEllipses[i].toPaintStd(tpanter);
        }
        for(int i=0;i<m_DispRotatedRects.size();i++)
        {
            m_DispRotatedRects[i].toPaintStd(tpanter);
        }
        for(int i=0;i<m_DispPolygons.size();i++)
        {
            m_DispPolygons[i].toPaintStd(tpanter);
        }
        for(int i=0;i<m_DispPolygonEs.size();i++)
        {
            m_DispPolygonEs[i].toPaintStd(tpanter);
        }
    }
    void toPaint(QPainter *tpanter,QRectF tRect,qreal SetScal)
    {
        QMutexLocker locker(&m_mutex);
        if(!m_MaskIm.isNull())
        {
            tpanter->drawImage(QPoint(0, 0),m_MaskIm);
        }
        for(int i=0;i<m_LabTxt.size();i++)
        {
            m_LabTxt[i].toPaint(tpanter,tRect,SetScal);
        }
        for(int i=0;i<m_DispTxt.size();i++)
        {
            m_DispTxt[i].toPaint(tpanter,SetScal);
        }
        for(int i=0;i<m_DispLineSegArow.size();i++)
        {
            m_DispLineSegArow[i].toPaint(tpanter,SetScal);
        }
        for(int i=0;i<m_DispPointS.size();i++)
        {
            //qDebug()<<"OOPII"<<m_DispPointS[i].ShowPos;
            m_DispPointS[i].toPaint(tpanter,SetScal);
        }
        for(int i=0;i<m_DispLineSegs.size();i++)
        {
            m_DispLineSegs[i].toPaint(tpanter,SetScal);
        }
        for(int i=0;i<m_DispLines.size();i++)
        {
            m_DispLines[i].toPaint(tpanter,SetScal);
        }
        for(int i=0;i<m_DispRects.size();i++)
        {
            m_DispRects[i].toPaint(tpanter,SetScal);
        }
        for(int i=0;i<m_DispCircles.size();i++)
        {
            m_DispCircles[i].toPaint(tpanter,SetScal);
        }
        for(int i=0;i<m_DispEllipses.size();i++)
        {
            m_DispEllipses[i].toPaint(tpanter,SetScal);
        }
        for(int i=0;i<m_DispRotatedRects.size();i++)
        {
            m_DispRotatedRects[i].toPaint(tpanter,SetScal);
        }
        for(int i=0;i<m_DispPolygons.size();i++)
        {
            m_DispPolygons[i].toPaint(tpanter,SetScal);
        }
        for(int i=0;i<m_DispPolygonEs.size();i++)
        {
            m_DispPolygonEs[i].toPaint(tpanter,SetScal);
        }
    }
    void AddByAnother(YtSetShowtObj *Obj)
    {
        QMutexLocker locker(&Obj->m_mutex);
        QMutexLocker lockerR(&m_mutex);

        m_LabTxt.append(Obj->m_LabTxt);
        m_DispTxt.append(Obj->m_DispTxt);
        m_DispLineSegArow.append(Obj->m_DispLineSegArow);
        m_DispPointS.append(Obj->m_DispPointS);
        m_DispLineSegs.append(Obj->m_DispLineSegs);
        m_DispLines.append(Obj->m_DispLines);
        m_DispRects.append(Obj->m_DispRects);
        m_DispCircles.append(Obj->m_DispCircles);
        m_DispEllipses.append(Obj->m_DispEllipses);
        m_DispRotatedRects.append(Obj->m_DispRotatedRects);
        m_DispPolygons.append(Obj->m_DispPolygons);
        m_DispPolygonEs.append(Obj->m_DispPolygonEs);
    }
    void append(QVector<LabTxt> &t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_LabTxt.append(t);
    }
    void append(LabTxt t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_LabTxt.append(t);
    }
    //
    void append(QVector<DispTxt> &t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispTxt.append(t);
    }
    void append(DispTxt t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispTxt.append(t);
    }
    //
    void append(QVector<DispLineSegArow> &t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispLineSegArow.append(t);
    }
    void append(DispLineSegArow t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispLineSegArow.append(t);
    }
    //
    void append(QVector<DispPointS> &t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispPointS.append(t);
    }
    void append(DispPointS t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispPointS.append(t);
    }
    //
    void append(QVector<DispLineSegs> &t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispLineSegs.append(t);
    }
    void append(DispLineSegs t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispLineSegs.append(t);
    }
    //
    void append(QVector<DispLines> &t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispLines.append(t);
    }
    void append(DispLines t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispLines.append(t);
    }
    //
    void append(QVector<DispRects> &t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispRects.append(t);
    }
    void append(DispRects t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispRects.append(t);
    }
    //
    void append(QVector<DispCircles> &t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispCircles.append(t);
    }
    void append(DispCircles t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispCircles.append(t);
    }
    //
    void append(QVector<DispEllipses> &t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispEllipses.append(t);
    }
    void append(DispEllipses t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispEllipses.append(t);
    }
    void append(QVector<DispRotatedRects> &t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispRotatedRects.append(t);
    }
    void append(DispRotatedRects t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispRotatedRects.append(t);
    }
    void append(QVector<DispPolygons> &t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispPolygons.append(t);
    }
    void append(DispPolygons t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispPolygons.append(t);
    }
    void append(QVector<DispPolygonEs> &t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispPolygonEs.append(t);
    }
    void append(DispPolygonEs t)
    {
        QMutexLocker lockerR(&m_mutex);
        m_DispPolygonEs.append(t);
    }
};


#endif // YTVISIONOVERPLAY_H
