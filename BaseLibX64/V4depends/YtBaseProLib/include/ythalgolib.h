#ifndef YTHALGOLIB_H
#define YTHALGOLIB_H

#include"ytvisiondefine.h"
#include <halconcpp/HalconCpp.h>
#include <halconcpp/HDevThread.h>
using namespace HalconCpp;
//获取测量边缘队找到的点集合
int Q_DECL_EXPORT YtCaliberRulePointT(HObject &inim, CMvLineSeg Seacerect, QVector<CMvPoint> &OutPoints, QVector<double> &OutGradient, CMvRotatedRect &OutFindRect, int thVal=20, double sigma=1.0);
//根据筛选条件条件输出最终点
int Q_DECL_EXPORT YtGetLineRstT(QVector<QVector<CMvPoint>> &inPoints,QVector<QVector<double>> &inGradient,QVector<CMvPoint> &RestGetPoints,QString tran,QString Sel);
//根据输入线段获取拟合线找点线段集
int Q_DECL_EXPORT YtCalclSider(CMvLineSeg insegLine,QVector<CMvLineSeg> &OutLines,int tSpitNumbe=10);
//获取平行线
int Q_DECL_EXPORT YtCalCleLineRest(QVector<QVector<CMvPoint>> &inPoints,QVector<QVector<double>> &inGradient,QString tranfirst,QString transec,double diflenmin,double diflenmax,QVector<CMvPoint> &outSectPoints,QVector<CMvPoint> &outSecPoints);

//根据点集合生成直线
int Q_DECL_EXPORT YtFitLineN(QVector<CMvPoint> &RestGetPoints,CMvLineSeg &restline,double xiangailv);

//根据点集合拟合圆:fitType=0->最小二乘拟合
int Q_DECL_EXPORT YtFitCircleN(QVector<CMvPoint> &RestGetPoints, CMvCircle &restCircle, int fitType=0);
//获取regon的边缘点，进行绘制
int Q_DECL_EXPORT toGetRegionPointNS(HObject &tReg, QVector<CMvPolygon> &toutPoint);
/**********************/
//新设计：找点算子
 class  Q_DECL_EXPORT YtHFindPointsDo
 {
 public:
     YtHFindPointsDo();
     ~YtHFindPointsDo();
     //新设计：找点算子
 private:
     //////////
     QString mMTran,mMSel;//传输计算以及点选择
     int mThVal;//阈值分割
     double mScore;//最小得分
     int mNumM;//卡尺个数
     double mSigma;//平滑阈值
     //////////////
     QVector<CMvPoint> m_GetPoints;
     QVector<CMvRotatedRect> m_OutRectc;
 public:
     enum MTran
     {
        //点计算模式
       WToB,//白到黑
       BToW,//黑到白
       TALL,//所有
       UniF,//统一
     };
     enum MSel
     {
         First,//第一个点
         End,//终点
         SALL,//所有
         Strangest,//最强点
     };
public:
     //设置测量值,NumM 把线段分割 NumM 个像素的 方格
     void toSetMeasurVal(int thval=30,MTran tMtran=MTran::WToB,MSel tMSel=MSel::First,double Sigma=1.0,double score=0.7);
     //获取测量参数值
     void toGetMeasurVal(int &thval,int &tMtran,int &tMSel,double &Sigma,double &score);
     //给一张图像输出点集合
     bool toGetPointsRestul(HObject &inim, QVector<CMvPoint> &OutPoint, CMvLineSeg inSegLine);
     //获取找到的边缘点
     void toGetRestPoints(QVector<CMvPoint> &OutP);
     //绘制查找线宽
     void toGetXldPoindS(QVector<CMvRotatedRect> &OutP);
 };
 //新设计：找线算子
 class  Q_DECL_EXPORT YtHFindLineNDo
 {
 public:
     YtHFindLineNDo();
     ~YtHFindLineNDo();
 private:
     //////////
     QString mMTran,mMSel;//传输计算以及点选择
     int mThVal;//阈值分割
     double mScore;//最小得分
     int mNumM;//卡尺个数
     double mSigma;//平滑阈值
     //////////////
     QVector<CMvPoint> m_GetPoints;
     QVector<CMvRotatedRect> m_OutRectc;
     QVector<CMvLineSeg> m_OutLineS;
//     YtHFindPointsDo m_findPoint;
 public:
     void toSetMeasurVal(int thval=30,YtHFindPointsDo::MTran tMtran=YtHFindPointsDo::MTran::WToB,\
                         YtHFindPointsDo::MSel tMSel=YtHFindPointsDo::MSel::First,int NumM=7,double Sigma=1.0,double score=0.7);
     //获取测量参数值
     void toGetMeasurVal(int &thval,int &tMtran,int &tMSel,int &NumM,double &Sigma,double &score);
     //给一张图像输出线
     bool toGetLineRestul(HObject &inim, CMvLineSeg &OutLine, CMvLineSeg inSegLine);
     //获取找到的边缘点
     void toGetRestPoints(QVector<CMvPoint> &OutP);
     //绘制查找线宽
     void toGetXldPoindS(QVector<CMvRotatedRect> &OutP);
     //绘制垂直查找线
     void toGetFindLines(QVector<CMvLineSeg> &OutL);


 };
 //矩形拟合算子（依据线拟合算子）
class  Q_DECL_EXPORT YtHFindRectDo
{
public:
    YtHFindRectDo();
    ~YtHFindRectDo();
private:
    //////////
    QString mMTran,mMSel;//传输计算以及点选择
    int mThVal;//阈值分割
    double mScore;//最小得分
    int mNumM;//卡尺个数
    double mLen;//卡尺长轴
    double mSigma;//平滑阈值
    int mFindtype;//搜索方向：0-内到外，1-外到内
    YtHFindLineNDo m_ytYtHFindLineNDo;//找线类
    CMvLineSeg m_findLines[4];
    double ImageWidth,ImageHight;
    QVector<CMvPoint> m_resultPoints;
    QVector<CMvRotatedRect> m_resultRotate;
public:
    CMvLineSeg m_resultLines[4];
    QVector<CMvPoint> m_IntersectPoints;//四条拟合边的交点集合
public:
    void toSetMeasurVal(int thval=30,YtHFindPointsDo::MTran tMtran=YtHFindPointsDo::MTran::WToB,\
                        YtHFindPointsDo::MSel tMSel=YtHFindPointsDo::MSel::First,int NumM=7,double RectLen=50,double Sigma=1.0,int infingType=0,double score=0.7);
    //获取测量参数值
    void toGetMeasurVal(int &thval,int &tMtran,int &tMSel,int &NumM,double &RectLen,double &Sigma,int &infingType,double &score);
    //给一张图像输出矩形
    bool toGetRectRestul(HObject &inim, CMvRotatedRect &OutRect, CMvRotatedRect &inRect);
    //根据输入旋转矩形输出找线的四边线段
    void toGetFourLinesByRect(CMvRotatedRect &inRect,int inFindtype=0);
   //获取线段俩俩焦点
    int  toGetLLPoint(CMvLineSeg inLine[], QVector<CMvPoint> &outInterseP);

    //获取找到的边缘点
    void toGetRestPoints(QVector<CMvPoint> &OutP);
    //绘制查找线宽
    void toGetXldPoindS(QVector<CMvRotatedRect> &OutP);

};
//圆形拟合算子(依据点卡尺）
class  Q_DECL_EXPORT YtHFindCircleDo
{
public:
    YtHFindCircleDo();
    ~YtHFindCircleDo();
private:
    QString mMTran,mMSel;//传输计算以及点选择
    int mThVal;//阈值分割
    int mNumM;//卡尺个数
    double mLen;//卡尺长轴
    double mSigma;//平滑阈值
    int mFindtype;//搜索方向：0-内到外，1-外到内
    YtHFindPointsDo m_findPointsDo;
    QVector<CMvLineSeg> m_findLines;
    QVector<CMvRotatedRect> m_FindRects;
    QVector<CMvPoint> m_resultPoints;
public:
    void toSetMeasurVal(int thval=30,YtHFindPointsDo::MTran tMtran=YtHFindPointsDo::MTran::WToB,\
                        YtHFindPointsDo::MSel tMSel=YtHFindPointsDo::MSel::First,int NumM=7,double RectLen=50,double Sigma=1.0,int infingType=0);
    //获取测量参数值
    void toGetMeasurVal(int &thval, int &tMtran, int &tMSel, int &NumM, double &RectLen, double &Sigma, int &infingType);
    //给一张图像输出圆形
    bool toGetCircleRestul(HObject &inim, CMvCircle &OutCircle, CMvCircle &inCircle);
    //根据输入圆形输出找点的测量边
    int toGetLinesByCircle(CMvCircle &inCircle,QVector<CMvLineSeg> &outfindLine,int inFindtype=0);
    //获取找到的边缘点
    void toGetRestPoints(QVector<CMvPoint> &OutP);
    //绘制查找线宽
    void toGetXldPoindS(QVector<CMvRotatedRect> &OutP);
};
//圆弧拟合算子(依据点卡尺）
class  Q_DECL_EXPORT YtHFindCircARC
{
public:
    YtHFindCircARC();
    ~YtHFindCircARC();
private:
    QString mMTran,mMSel;//传输计算以及点选择
    int mThVal;//阈值分割
    int mNumM;//卡尺个数
    double mLen;//卡尺长轴
    double mSigma;//平滑阈值
    int mFindtype;//搜索方向：0-内到外，1-外到内
    YtHFindPointsDo m_findPointsDo;
    QVector<CMvLineSeg> m_findLines;
    QVector<CMvRotatedRect> m_FindRects;
    QVector<CMvPoint> m_resultPoints;
public:
    void toSetMeasurVal(int thval=30,YtHFindPointsDo::MTran tMtran=YtHFindPointsDo::MTran::WToB,\
                        YtHFindPointsDo::MSel tMSel=YtHFindPointsDo::MSel::First,int NumM=7,double RectLen=50,double Sigma=1.0,int infingType=0);
    //获取测量参数值
    void toGetMeasurVal(int &thval, int &tMtran, int &tMSel, int &NumM, double &RectLen, double &Sigma, int &infingType);
    //给一张图像输出圆弧点
    bool toGetCirArcRestul(HObject &inim, QVector<CMvPoint>  &OutCirARCP, CMvArc &inCMvArc);
    //根据输入圆弧输出找点的测量边
    int toGetLinesByCirArc(CMvArc &inCirArc,QVector<CMvLineSeg> &outfindLine,int inFindtype=0);
    //获取找到的边缘点
    void toGetRestPoints(QVector<CMvPoint> &OutP);
    //绘制查找线宽
    void toGetXldPoindS(QVector<CMvRotatedRect> &OutP);
};
//构建一个ncc模板匹配的计算类(可以查找多个)
class Q_DECL_EXPORT YtFindNccModelN
{
public:
    YtFindNccModelN();
    ~YtFindNccModelN();
private:
    HTuple m_ModelID;//模型句柄

public:
    //创建模板用户需要提供，建模图像，此建模图像直接裁切出去的小图（单模板）
    bool toCreateNccMode(HObject &inim,double AngStart,double AngEnd,double AngStep,int NUmLevel);
    //创建模板用户需要提供，建模图像，此建模图像直接裁切出去的小图（多模板）
    bool toCreateNccModeS(QVector<HObject> &inim,double AngStart,double AngEnd,double AngStep,int NUmLevel);
    //根据输入图像查找(支持单模板查找多个，多模板查找)
    bool toFindNccModes(HObject &inim, double AngStart, double AngEnd, double minScor, double MaxOvePla, int NumMatchs, QVector<QVector<CMvPoint> > &OutPoint, QVector<QVector<double>> &OutAng);
    //保存NCC模板,提供一个路径即可
    bool toSaveNccModes(QString Path);
    //读取NCC模板，提供一个路径即可
    bool toLoadNccModes(QString Path);
};


//构建一个通用模板匹配的计算类(NCC、Shape可以查找多个)
class Q_DECL_EXPORT YtFindGeneralModel
{
public:
    YtFindGeneralModel();
    ~YtFindGeneralModel();
private:
    HTuple m_ModelID;//模型句柄
    double AngStart,AngEnd,AngStep,NUmLevel,ImScale;
    //inFindtype:0->NCC,1->shape,2->shapeXLD
    int NumMatchs,Contrast,Findtype;
    double ScaleMin,ScaleMax,minScor,MaxOvePla;
    QVector<HObject> m_Models;

public:
    void toSetMeasurVal(int inFindtype=0, double inImScale=1.0, double inAngStart=-180, double inAngEnd=180, double inAngStep=1, int inNUmLevel=0, int inNumMatchs=0, double inminScor=0.6, double inMaxOvePla=0.8, int inContrast=10, double inScaleMin=1, double inScaleMax=1.0);
    //获取建模参数值
    void toGetMeasurVal(int &inFindtype, double &inImScale, double &inAngStart, double &inAngEnd, double &inAngStep, int &inNUmLevel, int &inNumMatchs, double &inminScor, double &inMaxOvePla, int &inContrast, double &inScaleMin, double &inScaleMax);
    //创建模板用户需要提供，建模图像，此建模图像直接裁切出去的小图（单模板）
    bool toCreateGenerMode(HObject &inim);
    //创建模板用户需要提供，建模图像，此建模图像直接裁切出去的小图（多模板）
    bool toCreateGenerModeS(QVector<HObject> &inim);
    //根据输入图像查找(支持单模板查找多个，多模板查找)
    bool toFindGenerModes(HObject &inim, QVector<QVector<CMvPoint> > &OutPoint, QVector<QVector<double>> &OutAng, QVector<QVector<double> > &OutScore);
    //保存NCC模板,提供一个路径即可
    bool toSaveGenerModes(QString Path);
    //得到计算特征点
    bool toGetShapePoints(HObject &inm, HObject &Roi, QVector<CMvPolygon> &toutPoint);
    //读取NCC模板，提供一个路径即可
    bool toLoadGenerModes(QString Path, int fingType, QVector<HObject> &outModelInf, QVector<HObject> &outMask);
};
#endif // YTHALGOLIB_H
