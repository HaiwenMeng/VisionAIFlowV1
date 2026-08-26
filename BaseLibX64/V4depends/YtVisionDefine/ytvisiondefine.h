#ifndef YTVISIONDEFINE_H
#define YTVISIONDEFINE_H
#define YtSAPI __declspec (dllexport)
#include "math.h"
#include <QVector>
#include <QColor>
#include <QDataStream>
#include <QDebug>
#include <QCoreApplication>
#include <QDateTime>
//图像和视觉算法常用的单精度浮点型数据可忽略较小值的阈值
#define  MV_FEPS   1.0e-6F
//图像和视觉算法常用的双精度浮点型数据可忽略较小值的阈值
#define  MV_DEPS   1.0e-9
//2pi 的值
#define  MV_2_PI  6.283185307179586476925286766559
//pi 的值
#define  MV_PI   3.1415926535897932384626433832795
//pi/2 的值
#define  MV_PI_2   1.5707963267948966192313216916398
//角度转弧度
#define M_PI_180 0.01745329
//宏函数 延时
#define YtSleeP( msTime) {\
    QDateTime reach_time = QDateTime::currentDateTime().addMSecs(msTime);\
    while (QDateTime::currentDateTime() < reach_time)\
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);\
    }
//宏函数 安全删除指针
#define YtSafeDelete (pObject) {if(pObject){delete pObject;(pObject)=nullptr;}}
//宏函数 角度转弧度
#define YtD2R( Ang) {(Ang)*MV_PI/180.0}
//宏函数 弧度转角度
#define YtR2D(Ang)  {(Ang)*180.0/MV_PI}
//宏函数 两点式距离
#define YtDist2P(xs,ys,xe,ye ) (sqrt(((double)(xe) - (xs)) * ((xe) - (xs)) + ((double)(ye) - (ys)) * ((ye) - (ys))))
//宏函数MAX
#define YtMAX(a, b) ((a) > (b) ? (a) : (b))
//宏函数MIN
#define YtMIN(a, b) ((a) < (b) ? (a) : (b))
/////////////
//宏函数 获取编译时间
#define YtGetBuildTime QString("%1-%2-%3(%4)").arg(QString(__DATE__).mid(7,4))\
    .arg(QString("Jan#Feb#Mar#Apr#May#Jun#Jul#Aug#Sep#Oct#Nov#Dec").split("#").indexOf(QString(__DATE__).mid(0,3))+1)\
    .arg(QString(__DATE__).mid(4,2))\
    .arg(__TIME__);

//点定义
struct CMvPoint
{
    CMvPoint()
    {
        x=0.0;//点的x轴坐标，默认值为0.0
        y=0.0;//点的y轴坐标，默认值为0.0
    }
    CMvPoint(double tx,double ty)
    {
        x=tx;//点的x轴坐标，默认值为0.0
        y=ty;//点的y轴坐标，默认值为0.0
    }
    QVector<double> Data()
    {
        QVector<double> data;
        return data<<x<<y;
    }
    void GetData(QVector<double> data)
    {
        x=data.at(0);
        y=data.at(1);
    }
    double x=0.0;//点的x轴坐标，默认值为0.0
    double y=0.0;//点的y轴坐标，默认值为0.0
    friend QDataStream&operator >>(QDataStream &stream,CMvPoint &SetData)
    {
        stream>>SetData.x>>SetData.y;
        return stream;
    }
    //输出
    friend QDataStream&operator <<(QDataStream &stream,const CMvPoint &SetData)
    {
        stream<<SetData.x<<SetData.y;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvPoint &SetData)
    {

        debug<<QString("CMVPoint(%1,%2)").arg(SetData.x).arg(SetData.y);
        return debug;
    }
};
//三维点定义
struct CMvPoint3d
{
    double  x=0.0; //点的x轴坐标，默认值为0.0
    double  y=0.0;//点的y轴坐标，默认值为0.0
    double  z =0.0;//点的z轴坐标，默认值为0.0
    CMvPoint3d()
    {
        x=0.0;//点的x轴坐标，默认值为0.0
        y=0.0;//点的y轴坐标，默认值为0.0
        z=0.0;
    }
    CMvPoint3d(double tx,double ty,double tz)
    {
        x=tx;//点的x轴坐标，默认值为0.0
        y=ty;//点的y轴坐标，默认值为0.0
        z=tz;//点的y轴坐标，默认值为0.0

    }
    QVector<double> Data()
    {
        QVector<double> data;
        return data<<x<<y<<z;
    }
    void GetData(QVector<double> data)
    {
        x=data.at(0);
        y=data.at(1);
        z=data.at(2);
    }
    friend QDataStream&operator >>(QDataStream &stream,CMvPoint3d &SetData)
    {
        stream>>SetData.x>>SetData.y>>SetData.z;
        return stream;
    }
    friend QDataStream&operator <<(QDataStream &stream,const CMvPoint3d &SetData)
    {
        stream<<SetData.x<<SetData.y<<SetData.z;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvPoint3d &SetData)
    {

        debug<<QString("CMvPoint3d(%1,%2,%3)").arg(SetData.x).arg(SetData.y).arg(SetData.z);
        return debug;
    }

};
//直线定义
struct CMvLine
{
    CMvPoint CenterP;//线上的一个点
    double angle=0.0;//角度
    CMvLine()
    {

    }
    CMvLine(double tx,double ty,double tang)
    {
        CenterP.x=tx;
        CenterP.y=ty;
        angle=tang;
    }
    QVector<double> Data()
    {
        QVector<double> data;
        return data<<CenterP.x<<CenterP.y<<angle;
    }
    void GetData(QVector<double> data)
    {
        CenterP.x=data.at(0);
        CenterP.y=data.at(1);
        angle=data.at(2);
    }
    friend QDataStream&operator >>(QDataStream &stream,CMvLine &SetData)
    {
        stream>>SetData.CenterP.x>>SetData.CenterP.y>>SetData.angle;
        return stream;
    }
    friend QDataStream&operator <<(QDataStream &stream,const CMvLine &SetData)
    {
        stream<<SetData.CenterP.x<<SetData.CenterP.y<<SetData.angle;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvLine &SetData)
    {

        debug<<QString("CMvLine(%1,%2,%3)").arg(SetData.CenterP.x).arg(SetData.CenterP.y).arg(SetData.angle);
        return debug;
    }
};
//线段定义
struct CMvLineSeg
{
    CMvPoint  st ;//线段起始点，默认值为(0.0, 0.0)
    CMvPoint  ed; //线段终止点，默认值为(0.0, 0.0)
    double width;//可用可不用的一个参数
    CMvLineSeg()
    {
        st.x=0;
        st.y=0;
        ed.x=0;
        ed.y=0;
        width=0;
    }
    CMvLineSeg(double sx, double sy,double ex,double ey)
    {
        st.x=sx;
        st.y=sy;
        ed.x=ex;
        ed.y=ey;
    }
    CMvLineSeg(double sx, double sy,double ex,double ey,double twidth)
    {
        st.x=sx;
        st.y=sy;
        ed.x=ex;
        ed.y=ey;
        width=twidth;
    }
    double CenterX()
    {
        return (st.x+ed.x)/2;
    }
    double CenterY()
    {
        return (st.y+ed.y)/2;
    }
    double Length()
    {
        return sqrt((st.x-ed.x)*(st.x-ed.x)+(st.y-ed.y)*(st.y-ed.y));
    }
    double Deg()
    {

        return (180*Rad()/MV_PI);
    }
    double Rad()
    {
        return atan((ed.y-st.y)/(ed.x-st.x));
    }
    QVector<double> Data()
    {
        QVector<double> data;
        return data<<st.x<<st.y<<ed.x<<ed.y<<width;
    }
    void GetData(QVector<double> data)
    {
        st.x=data.at(0);
        st.y=data.at(1);
        ed.x=data.at(2);
        ed.y=data.at(3);
        width=data.at(4);
    }
    friend QDataStream&operator >>(QDataStream &stream,CMvLineSeg &SetData)
    {
        stream>>SetData.st>>SetData.ed>>SetData.width;
        return stream;
    }
    friend QDataStream&operator <<(QDataStream &stream,const CMvLineSeg &SetData)
    {
        stream<<SetData.st<<SetData.ed<<SetData.width;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvLineSeg &SetData)
    {

        debug<<QString("CMvLineSeg(%1,%2,%3,%4,%5)").arg(SetData.st.x).arg(SetData.st.y).arg(SetData.ed.x).arg(SetData.ed.y).arg(SetData.width);
        return debug;
    }
};
//圆定义
struct CMvCircle
{
    CMvCircle()
    {
        center.x=0;
        center.y=0;
        radius=0;
    }
    CMvCircle(double tx,double ty,double tradius)
    {
        center.x=tx;
        center.y=ty;
        radius=tradius;
    }
    CMvPoint  center;//圆心位置，默认值为(0.0, 0.0)
    double  radius=0.0 ;//半径长度，默认值为0.0
    QVector<double> Data()
    {
        QVector<double> data;
        return data<<center.x<<center.y<<radius;
    }
    void GetData(QVector<double> data)
    {
        center.x=data.at(0);
        center.y=data.at(1);
        radius=data.at(2);
    }
    friend QDataStream&operator >>(QDataStream &stream,CMvCircle &SetData)
    {
        stream>>SetData.center.x>>SetData.center.y>>SetData.radius;
        return stream;
    }

    friend QDataStream&operator <<(QDataStream &stream,const CMvCircle &SetData)
    {
        stream<<SetData.center.x<<SetData.center.y<<SetData.radius;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvCircle &SetData)
    {

        debug<<QString("CMvCircle(%1,%2,%3)").arg(SetData.center.x).arg(SetData.center.y).arg(SetData.radius);
        return debug;
    }
};
//圆弧定义
struct CMvArc
{
    CMvPoint  center ;//圆心位置，默认值为(0.0, 0.0)
    double  radius ;//圆弧半径长度，默认值为0.0
    double  stAngle ;//圆弧起始角度（弧度），默认值为0.0
    double  edAngle ;//圆弧结束角度（弧度），默认值为0.0
    CMvArc()
    {

    }
    CMvArc(double centerx,double centery,double tradius,double tstAngl,double tedAngle)
    {
        center.x=centerx;
        center.y=centery;
        radius=tradius;
        stAngle=tstAngl;
        edAngle=tedAngle;
    }
    double stAngleRad()
    {
        return (M_PI_180*stAngle);
    }
    double edAngleRad()
    {
        return (M_PI_180*edAngle);
    }
    QVector<double> Data()
    {
        QVector<double> data;
        return data<<center.x<<center.y<<radius<<stAngle<<edAngle;
    }
    void GetData(QVector<double> data)
    {
        center.x=data.at(0);
        center.y=data.at(1);
        radius=data.at(2);
        stAngle=data.at(3);
        edAngle=data.at(4);
    }
    friend QDataStream&operator >>(QDataStream &stream,CMvArc &SetData)
    {
        stream>>SetData.center.x>>SetData.center.y>>SetData.radius>>SetData.stAngle>>SetData.edAngle;
        return stream;
    }
    friend QDataStream&operator <<(QDataStream &stream,const CMvArc &SetData)
    {
        stream<<SetData.center.x<<SetData.center.y<<SetData.radius<<SetData.stAngle<<SetData.edAngle;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvArc &SetData)
    {

        debug<<QString("CMvArc(%1,%2,%3,%4,%5)").arg(SetData.center.x).arg(SetData.center.y).arg(SetData.radius)\
               .arg(SetData.stAngle).arg(SetData.edAngle);
        return debug;
    }
};
//坐标系定义
struct CMvCoord
{
    CMvPoint  origin;//坐标原点，默认值为(0.0, 0.0)
    double  angle;//x轴正方向的角度，默认值为0.0
    double Rad()
    {
        return (M_PI_180*angle);
    }
    QVector<double> Data()
    {
        QVector<double> data;
        return data<<origin.x<<origin.y<<angle;
    }
    void GetData(QVector<double> data)
    {
        origin.x=data.at(0);
        origin.y=data.at(1);
        angle=data.at(2);
    }
    friend QDataStream&operator >>(QDataStream &stream,CMvCoord &SetData)
    {
        stream>>SetData.origin.x>>SetData.origin.y>>SetData.angle;
        return stream;
    }
    friend QDataStream&operator <<(QDataStream &stream,const CMvCoord &SetData)
    {
        stream<<SetData.origin.x<<SetData.origin.y<<SetData.angle;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvCoord &SetData)
    {

        debug<<QString("CMvCoord(%1,%2,%3)").arg(SetData.origin.x).arg(SetData.origin.y).arg(SetData.angle);
        return debug;
    }

};
//椭圆定义
struct CMvEllipse
{
    CMvPoint  Center;//圆心位置，默认值为(0.0, 0.0)
    double  axisX ;//x半轴长度，默认值为0.0
    double  axisY;//y半轴长度，默认值为0.0
    double  angle;//x轴正方向的角度，默认值为0.0,这个是角度
    CMvEllipse()
    {

    }
    CMvEllipse(double centx,double centy,double tcx,double tcy,double tangle)
    {
        Center.x=centx;
        Center.y=centy;
        axisX=tcx;
        axisY=tcy;
        angle=tangle;
    }
    QVector<double> Data()
    {
        QVector<double> data;
        return data<<Center.x<<Center.y<<axisX<<axisY<<angle;
    }
    void GetData(QVector<double> data)
    {
        Center.x=data.at(0);
        Center.y=data.at(1);
        axisX=data.at(2);
        axisY=data.at(3);
        angle=data.at(4);
    }
    double Rad()
    {
        return (M_PI_180*angle);
    }
    double Deg()
    {
        return angle;
    }
    friend QDataStream&operator >>(QDataStream &stream,CMvEllipse &SetData)
    {
        stream>>SetData.Center.x>>SetData.Center.y>>SetData.axisX>>SetData.axisY>>SetData.angle;
        return stream;
    }
    friend QDataStream&operator <<(QDataStream &stream,const CMvEllipse &SetData)
    {
        stream<<SetData.Center.x<<SetData.Center.y<<SetData.axisX<<SetData.axisY<<SetData.angle;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvEllipse &SetData)
    {

        debug<<QString("CMvEllipse(%1,%2,%3,%4,%5)").arg(SetData.Center.x).arg(SetData.Center.y).arg(SetData.axisX)\
               .arg(SetData.axisY).arg(SetData.angle);
        return debug;
    }
};
//大小定义
struct CMvSize
{
    double  cx=0.0 ;//x轴方向宽度，默认值为0.0
    double  cy=0.0 ;//y轴方向宽度，默认值为0.0

    CMvSize()
    {

    }

    CMvSize(double tcx,double tcy)
    {
       cx = tcx;
       cy = tcy;
    }

    QVector<double> Data()
    {
        QVector<double> data;
        return data<<cx<<cy;
    }
    void GetData(QVector<double> data)
    {
        cx=data.at(0);
        cy=data.at(1);
    }
    friend QDataStream&operator >>(QDataStream &stream,CMvSize &SetData)
    {
        stream>>SetData.cx>>SetData.cy;
        return stream;
    }
    friend QDataStream&operator <<(QDataStream &stream,const CMvSize &SetData)
    {
        stream<<SetData.cx<<SetData.cy;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvSize &SetData)
    {

        debug<<QString("CMvSize(%1,%2)").arg(SetData.cx).arg(SetData.cy);
        return debug;
    }
};

//正矩形定义
struct CMvRect
{
    CMvPoint  LeftTop;//左上点位置，默认值为(0.0, 0.0)
    double  cx=0.0 ;//x轴方向宽度，默认值为0.0
    double  cy=0.0 ;//y轴方向宽度，默认值为0.0
    CMvRect()
    {

    }
    CMvRect(double lefttopx,double lefttopy,double tcx,double tcy)
    {
        LeftTop.x=lefttopx;
        LeftTop.y=lefttopy;
        cx=tcx;
        cy=tcy;
    }
    QVector<double> Data()
    {
        QVector<double> data;
        return data<<LeftTop.x<<LeftTop.y<<cx<<cy;
    }
    void GetData(QVector<double> data)
    {
        LeftTop.x=data.at(0);
        LeftTop.y=data.at(1);
        cx=data.at(2);
        cy=data.at(3);

    }
    friend QDataStream&operator >>(QDataStream &stream,CMvRect &SetData)
    {
        stream>>SetData.LeftTop.x>>SetData.LeftTop.y>>SetData.cx>>SetData.cy;
        return stream;
    }
    friend QDataStream&operator <<(QDataStream &stream,const CMvRect &SetData)
    {
        stream<<SetData.LeftTop.x<<SetData.LeftTop.y<<SetData.cx<<SetData.cy;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvRect &SetData)
    {

        debug<<QString("CMvRect(%1,%2,%3,%4)").arg(SetData.LeftTop.x).arg(SetData.LeftTop.y).arg(SetData.cx).arg(SetData.cy);
        return debug;
    }
};
//旋转矩形定义
struct CMvRotatedRect
{
    CMvPoint  Center;//中心点位置，默认值为(0.0, 0.0)
    double  cx=0.0 ;//x轴方向宽度，默认值为0.0
    double  cy=0.0 ;//y轴方向宽度，默认值为0.0
    double angle=0.0;//旋转角度，弧度
    CMvRotatedRect()
    {

    }
    CMvRotatedRect(double centx,double centy,double tcx,double tcy,double tangle)
    {
        Center.x=centx;
        Center.y=centy;
        cx=tcx;
        cy=tcy;
        angle=tangle;
    }
    double Rad()
    {
        return (M_PI_180*angle);
    }
    double Deg()
    {
        return angle;
    }
    QVector<double> Data()
    {
        QVector<double> data;
        return data<<Center.x<<Center.y<<cx<<cy<<angle;
    }
    void GetData(QVector<double> data)
    {
        Center.x=data.at(0);
        Center.y=data.at(1);
        cx=data.at(2);
        cy=data.at(3);
        angle=data.at(4);
    }
    void toGetPoint(CMvPoint (&GetPoints)[4])
    {
        double fAngle = -M_PI_180*angle;
        double a = sin(fAngle) * 0.5;
        double b = cos(fAngle) * 0.5;
        double tcx=Center.x;
        double tcy=Center.y;
        double h=this->cy*2;
        double w=this->cx*2;
        GetPoints[0].x=(tcx + a * h - b * w);
        GetPoints[0].y=(tcy - b * h - a * w);
        GetPoints[2].x=(2 * tcx - GetPoints[0].x);
        GetPoints[2].y=(2 * tcy - GetPoints[0].y);

        GetPoints[3].x=(tcx - a * h - b * w);
        GetPoints[3].y=(tcy + b * h - a * w);
        GetPoints[1].x=(2 * tcx - GetPoints[3].x);
        GetPoints[1].y=(2 * tcy - GetPoints[3].y);
    }
    friend QDataStream&operator >>(QDataStream &stream,CMvRotatedRect &SetData)
    {
        stream>>SetData.Center.x>>SetData.Center.y>>SetData.cx>>SetData.cy>>SetData.angle;
        return stream;
    }
    friend QDataStream&operator <<(QDataStream &stream,const CMvRotatedRect &SetData)
    {
        stream<<SetData.Center.x<<SetData.Center.y<<SetData.cx<<SetData.cy<<SetData.angle;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvRotatedRect &SetData)
    {

        debug<<QString("CMvRotatedRect(%1,%2,%3,%4,%5)").arg(SetData.Center.x).arg(SetData.Center.y).arg(SetData.cx).arg(SetData.cy).arg(SetData.angle);
        return debug;
    }
};
//腰圆定义
struct CMvWaistRound
{
    CMvPoint  Center;//中心点位置，默认值为(0.0, 0.0)
    double  cx=0.0 ;//x轴方向宽度，默认值为0.0,再绘制半径即可
    double  cy=0.0 ;//y轴方向宽度，默认值为0.0
    double angle=0.0;//旋转角度，弧度
    CMvWaistRound()
    {

    }
    CMvWaistRound(double centx,double centy,double tcx,double tcy,double tangle)
    {
        Center.x=centx;
        Center.y=centy;
        cx=tcx;
        cy=tcy;
        angle=tangle;
    }
    double Rad()
    {
        return (M_PI_180*angle);
    }
    QVector<double> Data()
    {
        QVector<double> data;
        return data<<Center.x<<Center.y<<cx<<cy<<angle;
    }
    void GetData(QVector<double> data)
    {
        Center.x=data.at(0);
        Center.y=data.at(1);
        cx=data.at(2);
        cy=data.at(3);
        angle=data.at(4);
    }
    friend QDataStream&operator >>(QDataStream &stream,CMvWaistRound &SetData)
    {
        stream>>SetData.Center.x>>SetData.Center.y>>SetData.cx>>SetData.cy>>SetData.angle;
        return stream;
    }
    friend QDataStream&operator <<(QDataStream &stream,const CMvWaistRound &SetData)
    {
        stream<<SetData.Center.x<<SetData.Center.y<<SetData.cx<<SetData.cy<<SetData.angle;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvWaistRound &SetData)
    {

        debug<<QString("CMvWaistRound(%1,%2,%3,%4,%5)").arg(SetData.Center.x).arg(SetData.Center.y).arg(SetData.cx).arg(SetData.cy).arg(SetData.angle);
        return debug;
    }
};
//圆环定义
struct CMvRingCircle
{
    CMvPoint  Center;//中心点位置，默认值为(0.0, 0.0)
    double RadisMin=10;//圆环内半径
    double RadisMax=20;//圆环外半径
    CMvRingCircle()
    {

    }
    CMvRingCircle(double centx,double centy,double tmin,double tmax)
    {
        Center.x=centx;
        Center.y=centy;
        RadisMax=tmax;
        RadisMin=tmin;
    }
    QVector<double> Data()
    {
        QVector<double> data;
        return data<<Center.x<<Center.y<<RadisMin<<RadisMax;
    }
    void GetData(QVector<double> data)
    {
        Center.x=data.at(0);
        Center.y=data.at(1);
        RadisMin=data.at(2);
        RadisMax=data.at(3);
    }
    friend QDataStream&operator >>(QDataStream &stream,CMvRingCircle &SetData)
    {
        stream>>SetData.Center.x>>SetData.Center.y>>SetData.RadisMin>>SetData.RadisMax;
        return stream;
    }
    friend QDataStream&operator <<(QDataStream &stream,const CMvRingCircle &SetData)
    {
        stream<<SetData.Center.x<<SetData.Center.y<<SetData.RadisMin<<SetData.RadisMax;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvRingCircle &SetData)
    {

        debug<<QString("CMvRingCircle(%1,%2,%3,%4)").arg(SetData.Center.x).arg(SetData.Center.y).arg(SetData.RadisMin).arg(SetData.RadisMax);
        return debug;
    }
};
//多边形定义
struct CMvPolygon
{
    QVector<CMvPoint> points;//点集合
    CMvPolygon()
    {

    }
    CMvPolygon(QVector<CMvPoint> &tpoints)
    {
        points=tpoints;
    }
    void toClearData()
    {
        points.clear();
    }
    QVector<double> Data()
    {
        QVector<double> data;
        for(int i=0;i<points.size();i++)
        {

            data<<points.at(i).x<<points.at(i).y;
        }
        return data;

    }
    void GetData(QVector<double> data)
    {
        points.clear();
        for(int i=0;i<data.size()/2;i++)
        {
            CMvPoint tp;
            tp.x=data.at(2*i);
            tp.y=data.at(2*i+1);
            points.push_back(tp);
        }
    }
    friend QDataStream&operator >>(QDataStream &stream,CMvPolygon &SetData)
    {
        QVector<double> data;
        stream>>data;
        SetData.GetData(data);
        return stream;
    }
    friend QDataStream&operator <<(QDataStream &stream, CMvPolygon &SetData)
    {
        QVector<double> data=SetData.Data();
        stream<<data;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvPolygon &SetData)
    {

        debug<<"CMvPolygon(";

        for(int i=0;i<SetData.points.size();i++)
        {
            debug<<SetData.points.at(i);
        }
        debug<<")";
        return debug;
    }
};
//扇形定义
struct CMvPie
{
    CMvPoint  center ;//中心点位置，默认值为(0.0, 0.0)
    double RadisMin=10;//圆环内半径
    double RadisMax=20;//圆环外半径
    double stAngle ;//圆弧起始角度（弧度），默认值为0.0
    double edAngle ;//圆弧结束角度（弧度），默认值为0.0
    CMvPie()
    {

    }
    CMvPie(double centx,double centy,double tmin,double tmax,double tstang,double tedAngel)
    {
        center.x=centx;
        center.y=centy;
        RadisMax=tmax;
        RadisMin=tmin;
        stAngle=tstang;
        edAngle=tedAngel;
    }
    QVector<double> Data()
    {
        QVector<double> data;
        return data<<center.x<<center.y<<RadisMin<<RadisMax<<stAngle<<edAngle;
    }
    void GetData(QVector<double> data)
    {
        center.x=data.at(0);
        center.y=data.at(1);
        RadisMin=data.at(2);
        RadisMax=data.at(3);
        stAngle=data.at(4);
        edAngle=data.at(5);
    }
    friend QDataStream&operator >>(QDataStream &stream,CMvPie &SetData)
    {
        stream>>SetData.center.x>>SetData.center.y>>SetData.RadisMin>>SetData.RadisMax>>SetData.stAngle>>SetData.edAngle;
        return stream;
    }
    friend QDataStream&operator <<(QDataStream &stream,const CMvPie &SetData)
    {
        stream<<SetData.center.x<<SetData.center.y<<SetData.RadisMin<<SetData.RadisMax<<SetData.stAngle<<SetData.edAngle;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const CMvPie &SetData)
    {

        debug<<QString("CMvPie(%1,%2,%3,%4,%5,%6)").arg(SetData.center.x).arg(SetData.center.y).arg(SetData.RadisMin).arg(SetData.RadisMax)\
               .arg(SetData.stAngle).arg(SetData.edAngle);
        return debug;
    }
};







#endif // YTVISIONDEFINE_H
