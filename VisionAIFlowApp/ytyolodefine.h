#ifndef YTYOLODEFINE_H
#define YTYOLODEFINE_H
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPointF>
#include <QSize>
#include "ytvisiondefine.h"
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardItemModel>
#include <QListWidgetItem>
#include <QProcess>
#include "YtVisionOverPlay.h"
/*
 1：这个类需要完成 json 类似xlabel的标注格式，这样就可以调用xlabel的标注
 2：这样使用原有python的转换脚本就可以生成任意我们想生成的格式，不用自己去写了
 3：标注本身确实不是很需要增广，有辅助标注的情况下，不需要增广，而且本身yolo的
 * 训练，里面自带了增广，这样整个程序的工作量会好很多，而且本身如果不是切割的标注
 * 可以共用标注文件，就不需要再生成一遍，再生成一遍也是很有必要，毕竟图象格式需要锁定
 * 而且利用python环境我们可以训练很多其他的模型
 *
 *
 *
 *
*/
//获取点集的外包轮廓
CMvRect toGetPointS(QVector<CMvPoint> GetPos);

CMvRotatedRect toGetPRota(QVector<CMvPoint> GetPos);

struct LabelSet
{
    enum LROIShape {
        LcircleROI = 0,      //圆形ROI
        LellipseROI = 1,     //椭圆ROI
        LrectangleROI = 2,   //矩形ROI
        LrotaterectangleROI = 3,//旋转矩形ROI
        LlineSegCabROI = 4,        //直线ROI卡尺
        LarcROI = 5,         //弧形ROI
        LringROI = 6,        //圆环ROI
        LpolygonROI = 7,     //多边形ROI
        LwaistROI = 8,       //腰形ROI
        LpieROI = 9,          //扇形ROI
        LpointROI = 10 ,       //点ROI
        LpolygonROIE = 11,     //多边ROI,非循环点集
        LlineSegROI = 12,        //线段

    };
    QString Name;
    QString RoiData;//Roi数据，用于显示
    QString shape_type;

    //反解析数据
    QVector<double> toGetRoiData()
    {
        QVector<double> temdata;
        QStringList setlis=RoiData.split("#");
        for(int i=0;i<setlis.size();i++)
        {
            temdata.append(setlis[i].toDouble());
        }
        return temdata;
    }
    QString toGetName()
    {
        return Name;
    }
    //解析类型
    int toGetRoiType()
    {
        QStringList setlis;
        setlis<<"rectangle"<<"rotation"<<"point"<<"polygon"<<"circle"<<"line";
        QVector<int> shapedefine;
        shapedefine<<LrectangleROI<<LrotaterectangleROI<<LpointROI<<LpolygonROI<<LcircleROI<<LlineSegROI;
        int getindex=setlis.indexOf(shape_type);
        //qDebug()<<"GetName"<<shape_type<<getindex;
        if(getindex>=0)
        {
            return shapedefine[getindex];
        }
        return LrectangleROI;
    }
    //内部的点数据
    QVector<QPointF> toGetPoints()
    {
        QVector<QPointF> OutPoints;
        switch (toGetRoiType())
        {
        case LrectangleROI:
        {
            CMvRect temdata;
            temdata.GetData(toGetRoiData());
            //获取四个点
            OutPoints<<QPointF(temdata.LeftTop.x,temdata.LeftTop.y)
                    <<QPointF(temdata.LeftTop.x+temdata.cx,temdata.LeftTop.y)
                   <<QPointF(temdata.LeftTop.x+temdata.cx,temdata.LeftTop.y+temdata.cy)
                  <<QPointF(temdata.LeftTop.x,temdata.LeftTop.y+temdata.cy);

            break;
        }
        case LrotaterectangleROI:
        {
            CMvRotatedRect temdata;
            temdata.GetData(toGetRoiData());
            //
            CMvPoint getpp[4];
            temdata.toGetPoint(getpp);
            OutPoints<<QPointF(getpp[0].x,getpp[0].y)
                    <<QPointF(getpp[1].x,getpp[1].y)
                    <<QPointF(getpp[2].x,getpp[2].y)
                    <<QPointF(getpp[3].x,getpp[3].y);



            break;
        }
        case LpointROI:
        {
            CMvPoint temdata;
            temdata.GetData(toGetRoiData());
            OutPoints<<QPointF(temdata.x,temdata.y);
            break;
        }
        case LpolygonROI:
        {
            CMvPolygon temdata;
            temdata.GetData(toGetRoiData());
            for(int i=0;i<temdata.points.size();i++)
            {
                OutPoints<<QPointF(temdata.points[i].x,temdata.points[i].y);
            }

            break;
        }
        case LcircleROI:
        {
            CMvCircle temdata;
            temdata.GetData(toGetRoiData());
            OutPoints<<QPointF(temdata.center.x,temdata.center.y)
                    <<QPointF(temdata.center.x+temdata.radius,temdata.center.y);
            break;
        }
        case LlineSegROI:
        {
            CMvLineSeg temdata;
            temdata.GetData(toGetRoiData());
            OutPoints<<QPointF(temdata.st.x,temdata.st.y)
                    <<QPointF(temdata.ed.x,temdata.ed.y);
            break;
        }
        default:
            break;
        }

        return OutPoints;


    }
    bool toInitProData(QString SetName,int RoiShape,QVector<double> Data)
    {
         Name=SetName;
         QStringList setlis;
         setlis<<"rectangle"<<"rotation"<<"point"<<"polygon"<<"circle"<<"line";
         QVector<int> shapedefine;
         shapedefine<<LrectangleROI<<LrotaterectangleROI<<LpointROI<<LpolygonROI<<LcircleROI<<LlineSegROI;
         int getindex=shapedefine.indexOf(RoiShape);
         //qDebug()<<"GetName"<<shape_type<<getindex;
         if(getindex>=0)
         {
             shape_type=setlis.at(getindex);
         }
         else
         {
             shape_type="rectangle";
         }
         //
         for(int j=0;j<Data.size();j++)
         {
             if(j==0)
             {
                 RoiData=QString::number(Data.at(j),'f',4);
             }
             else
             {
                 RoiData+="#"+QString::number(Data.at(j),'f',4);
             }
         }
         return true;
    }
    //反序列化
    bool toInitData(QString SetName,QString RoiShape,QVector<QPointF> SetPoints,double tdirection=0)
    {
        //QString Name;
        //QString RoiData;//Roi数据，用于显示
        //QString shape_type;
        Name=SetName;
        shape_type=RoiShape;
        int shapesindex=toGetRoiType();
        //qDebug()<<shapesindex<<RoiShape<<SetPoints.size()<<tdirection<<"XXXXXXXXXX";
        QVector<double> OutData;
        switch (shapesindex)
        {

        case LrectangleROI:
        {
            if(SetPoints.size()!=4)
            {
                return false;
            }
            CMvRect temdata;
            temdata.LeftTop=CMvPoint(SetPoints[0].x(),SetPoints[0].y());
            temdata.cx=fabs(SetPoints[2].x()-SetPoints[0].x());
            temdata.cy=fabs(SetPoints[2].y()-SetPoints[0].y());
            OutData=temdata.Data();
            break;
        }
        case LrotaterectangleROI:
        {

            if(SetPoints.size()!=4)
            {
                return false;
            }


            CMvRotatedRect temdata;
            //怎么解析 再说
            temdata.angle=360-tdirection/MV_PI*180;
            //
            temdata.Center.x=(SetPoints[0].x()+SetPoints[1].x()+SetPoints[2].x()+SetPoints[3].x())/4;
            temdata.Center.y=(SetPoints[0].y()+SetPoints[1].y()+SetPoints[2].y()+SetPoints[3].y())/4;
            //
            temdata.cx=YtDist2P(SetPoints[0].x(),SetPoints[0].y(),SetPoints[1].x(),SetPoints[1].y())/2;
            temdata.cy=YtDist2P(SetPoints[0].x(),SetPoints[0].y(),SetPoints[3].x(),SetPoints[3].y())/2;

            OutData=temdata.Data();

            break;
        }
        case LpointROI:
        {
            if(SetPoints.size()!=1)
            {
                return false;
            }
            CMvPoint temdata;
            temdata=CMvPoint(SetPoints[0].x(), SetPoints[0].y());
            OutData=temdata.Data();
            break;
        }
        case LpolygonROI:
        {
            if(SetPoints.size()<3)
            {
                return false;
            }
            CMvPolygon temdata;
            for(int j=0;j<SetPoints.size();j++)
            {
                temdata.points.append(CMvPoint(SetPoints[j].x(),SetPoints[j].y()));
            }
            OutData=temdata.Data();

            break;
        }
        case LcircleROI:
        {
            if(SetPoints.size()!=2)
            {
                return false;
            }
            CMvCircle temdata;
            temdata.center=CMvPoint(SetPoints[0].x(), SetPoints[0].y());
            temdata.radius=YtDist2P(SetPoints[0].x(), SetPoints[0].y(),SetPoints[1].x(), SetPoints[1].y());
            OutData=temdata.Data();
            break;
        }
        case LlineSegROI:
        {
            if(SetPoints.size()!=2)
            {
                return false;
            }
            CMvLineSeg temdata;
            temdata.st=CMvPoint(SetPoints[0].x(), SetPoints[0].y());
            temdata.ed=CMvPoint(SetPoints[1].x(), SetPoints[1].y());
            OutData=temdata.Data();
//            foreach(double d, OutData)
//            {
//                qDebug()<<"line temdata:"<<d;
//            }
            break;
        }
        default:
            break;
        }
        if(OutData.size()>0)
        {
            for(int j=0;j<OutData.size();j++)
            {
                if(j==0)
                {
                    RoiData=QString::number(OutData.at(j),'f',4);
                }
                else
                {
                    RoiData+="#"+QString::number(OutData.at(j),'f',4);
                }
            }
            qDebug()<<"Name:"<<Name<<" shape_type:"<<shape_type<<" RoiData:"<<RoiData;
            return true;
        }
        return false;
    }
};
class YtRoiLabelSet
{
public:
    YtRoiLabelSet();
public:
    QVector<LabelSet> m_SetLabeset;//存储的标注集
    int m_imWidth;
    int m_imHight;
    QString m_imagePath;//图象名称，
    QStringList m_NameSetLis;

public:
    void toSaveJson(QString Path);//存储json
    void toLoadJson(QString Path);//按照文件名，去这个目录找json文件
    void toLoadJsonFile(QString FileName);
    void toAppendData(LabelSet LabelName);
    void toInserData(LabelSet LabelName,int index);

    void toModify(LabelSet LabelName, int setindex, bool ischangename=false);
    void toReMoveIndex(int setindex);//移除指定
    void toClearData();
    void toMovePos(int xPos,int yPos);
public:
    //根据目录给出保存txt文本
    void toSaveDetcTxt(QString Path,QVector<QString> NameList,QSize CropSize=QSize(100,100));
    void toGenDetcOveplay(YtSetShowtObj &setshow, QVector<QString> NameList, QVector<QColor> ColorLis, QSize CropSize=QSize(100,100));
    void toLoadDetcTxt(QString filename,QSize imsize,QVector<QString> NameList);//输入文件名和列表就行
    //根据目录给出旋转矩形的txt文本
    void toSaveObbTxt(QString Path,QVector<QString> NameList,QSize CropSize=QSize(100,100));
    void toGenObbOveplay(YtSetShowtObj &setshow, QVector<QString> NameList, QVector<QColor> ColorLis, QSize CropSize=QSize(100,100));
    void toLoadObbTxt(QString filename,QSize imsize,QVector<QString> NameList);//输入文件名和列表就行

    //更具目录给出语义分割的txt文本
    void toSaveSegTxt(QString Path,QVector<QString> NameList,QSize CropSize=QSize(100,100));
    void toGenSegOveplay(YtSetShowtObj &setshow, QVector<QString> NameList, QVector<QColor> ColorLis, QSize CropSize=QSize(100,100));
    void toLoadSegTxt(QString filename,QSize imsize,QVector<QString> NameList);//输入文件名和列表就行
    //根据分类识别去解析txt文本
    void toSaveClassTxt(QString Path,QVector<QString> NameList,QSize CropSize=QSize(100,100));
    void toGenClassOveplay(YtSetShowtObj &setshow, QVector<QString> NameList, QVector<QColor> ColorLis, QSize CropSize=QSize(100,100));
    void toLoadClassTxt(QString filename,QSize imsize,QVector<QString> NameList);//输入文件名和列表就行
    QString toLoadOcrTxt(QString filename,QSize imsize,QVector<QString> NameList);
    //去加载文本目标的数据接口
    void toSaveFourPosTxt(QString Path,QVector<QString> NameList,QSize CropSize=QSize(100,100));
    void toGenFourPosOveplay(YtSetShowtObj &setshow, QVector<QString> NameList, QVector<QColor> ColorLis, QSize CropSize=QSize(100,100));
    void toLoadFourPosTxt(QString filename,QSize imsize,QVector<QString> NameList);//输入文件名和列表就行
public:
    void toSetImSize(QSize ImSize=QSize(640,640));
    QSize toGetImSize();
    void toSetFileName(QString filename);
    QString toGetFileName();//去除尾缀
};

//这是一个静态类，用于提供一些常规变量管理的处理
class YtYoloDefine
{
    //对于xlabel 可以选到次级目录
public:
    //这里数据就用json 管理
    //获取工作目录
    static QString toGetWorkPath();
    //设置工作目录
    static void toSetWorkPath(QString WorkPath);

    //获取Python工作目录
    static QString toGetPythonPath();
    //设置Python工作目录
    static void toSetPythonPath(QString PythPath);

    //
    //定义几个目录的地址
    static QString toGetLabelPath();
    static QString toGetDataPath();
    static QString toGetTrainPath();
    static QString toGetValuePath();
    //定义几个常规函数,获取子目录下的文件夹
    static QFileInfoList toGetPathDirInfo(QString Path);
    //获取目录下指定格式的文件
    static QFileInfoList toGetPathFileInfo(QString Path,QStringList FilterName);
    //获取目录下指定格式的文件,这个文件夹两个格式的文件都存在，还是存在1不存在2的判断
    static QFileInfoList toGetPathSpecialFileInfo(QString Path,QStringList FilterName1,QString FilterName2,bool isexit=true);
    //写系统steting
    static void toSaveSetting(QString WorkingPath);
    //看是否存在字体文件
    static void toProFontData();

};
//定义一个访问当前标注以及说明的管理类
class YtYoloSetPro
{
public:
    YtYoloSetPro();
public:
    QString m_InfoSet;
    QVector<QString> m_NameList;
    QVector<QColor>  m_ClorDefine;
    QStringList m_UncheckedDataSheetList;

public:
    void toLoadData(QString Path);
    void toSaveData(QString Path);
public:
    void toSetInfoSet(QString InfoSet);
    QString toGetInfoSet();
    //
    void toSetLabelInfo(QVector<QString> NameList,QVector<QColor> ClorDefine);
    void toAppendLabelInfo(QString NameList,QColor ClorDefine);
    void toModifyInfo(QString NameList,QColor ClorDefine,int index=0);

    QVector<QString> toGetLabelName();
    QVector<QColor> toGetColorDefine();



};
//构造一个训练参数的
class ProTrainData
{
public:
    ProTrainData();
    //
public:
    enum YtDlType
    {
        YtDetect,           //正矩形目标识别   0
        YtRotateDet,        //斜矩形目标识别   1 ,对应YOLO里的OBB(Oriented Bounding Box)概念
        YtSegment,          //语义分割      2
    };
    enum YtDlSize
    {
        Ytn,           //正矩形目标识别   0
        Yts,        //斜矩形目标识别   1 ,对应YOLO里的OBB(Oriented Bounding Box)概念
        Ytm,          //语义分割      2
        Ytl,          //语义分割      2
        Ytx,          //语义分割      2
    };
public:
    int         m_ModelSize = Ytn;
    int         m_TaskType = YtDetect;
    int         m_EpochNum = 100;
    int         m_ImageSize = 640;
    bool        m_isMultiScal=false;//多尺度
    bool        m_Isint8=false;//半精度
    int         m_WorksThread=8;//
    int         m_BathSize=16;//
    int         m_isMosick=10;//数据增强轮数
    int         m_SetImagechgle=1;
public:
    void toSaveData(QString SavePath);//保存json
    void toLoadData(QString LoadPath);//读取json
public:
    int toGetChangle();
    QString toGetCmd();
    QString toGetTrainCmd(QString processName, QString PretrainedModelPath="");
    QString toGeExportCmd(QString processName);
    QString toGeExportOpenVinoInt8Cmd(QString processName);
    static bool IsJsonHasLabel(QString JsonPath, QString LabelName);    //解析json

    QString toGeExportCmdBatch(QString processName, int BatchSize=4);
    QString toGetQuantCmd(QString processName);



};
#endif // YTYOLODEFINE_H
