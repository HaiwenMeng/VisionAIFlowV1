#ifndef YTALGOBASEPRO_H
#define YTALGOBASEPRO_H
#include "ythalgolib.h"
#include "QtGlobal"
struct ModeSetPar
{
    double m_AngStart,m_AngEnd,m_AngStep;//角度定义
    double m_NUmLevel;//金字塔层数，<1 为atuo
    double m_ImScale;//缩放系数
    double m_ScaleMin,m_ScaleMax;//形变系数
    double m_minScor;//最小匹配得分
    double m_MaxOvePla;//重叠率
    int m_NumMatchs;//匹配数量
    int m_Contrast;//检测梯度阈值
    int m_Findtype;//Findtype:0->NCC,1->shape,2->shapeXLD
    bool IsChange(ModeSetPar &NewPar)
    {
        bool ischange=false;
        if(fabs(m_AngStart-NewPar.m_AngStart)>0.01)
        {
            ischange=true;
        }
        m_AngStart=NewPar.m_AngStart;
        //
        if(fabs(m_AngEnd-NewPar.m_AngEnd)>0.01)
        {
            ischange=true;
        }
        m_AngEnd=NewPar.m_AngEnd;
        //
        if(fabs(m_AngStep-NewPar.m_AngStep)>0.01)
        {
            ischange=true;
        }
        m_AngStep=NewPar.m_AngStep;
        //
        if(fabs(m_NUmLevel-NewPar.m_NUmLevel)>0.01)
        {
            ischange=true;
        }
        m_NUmLevel=NewPar.m_NUmLevel;
        //
        if(fabs(m_ImScale-NewPar.m_ImScale)>0.01)
        {
            ischange=true;
        }
        m_ImScale=NewPar.m_ImScale;
        //
        if(fabs(m_ScaleMin-NewPar.m_ScaleMin)>0.01)
        {
            ischange=true;
        }
        m_ScaleMin=NewPar.m_ScaleMin;
        //
        if(fabs(m_ScaleMax-NewPar.m_ScaleMax)>0.01)
        {
            ischange=true;
        }
        m_ScaleMax=NewPar.m_ScaleMax;
        //
        m_minScor=NewPar.m_minScor;
        m_MaxOvePla=NewPar.m_MaxOvePla;
        m_NumMatchs=NewPar.m_NumMatchs;

        //
        if(m_Contrast!=NewPar.m_Contrast)
        {
            ischange=true;
        }
        m_Contrast=NewPar.m_Contrast;
        //
        if(m_Findtype!=NewPar.m_Findtype)
        {
            ischange=true;
        }
        m_Findtype=NewPar.m_Findtype;

        return ischange;

    }

};
//查找类
class Q_DECL_EXPORT YtFindModePro
{
public:
    YtFindModePro();
    ~YtFindModePro();
public:
    HTuple m_ModelID;//模型句柄
public:
    ModeSetPar m_SetModePar;//建模参数
    QString ErrMsg;
    QVector<CMvPoint> m_GetPoints;
    QVector<double>   m_GetAng;
    QVector<double>   m_getScore;//查找分数
    CMvPoint m_OffSetPos;
public:
    HObject m_ShapeReginon;
public:
    void toClearMode();//清除
    bool toSetModeVar(ModeSetPar &DetcPar,QString ModePath);//执行初始化创建
    void toGetModeVar(ModeSetPar &DetcPar);
    bool toExecFind(HObject &InImage);
    void toGetRest(QVector<CMvPoint> &GetPoints,QVector<double> &getAng,QVector<double> &getScore);
    void toGetRestReg(HObject &RestReg);
    QString toGetErrMsg();

};

//通用仿设类
class Q_DECL_EXPORT YtHomtAxisPro
{
    //坐标系统一类
public:
    YtHomtAxisPro();
    ~YtHomtAxisPro();
public:
    //因为只是一个处理类，所以就不存在模板这个概念
    HTuple m_HomMat2D;//转换对象,正向转换
    HTuple m_HomMat2DD;//反向转化，比如把图像反向修正
public:
    //这里的角度为弧度
    bool toGenHomMat2D(CMvPoint InP,double InAng,CMvPoint GetP,double GetAng);
    //
    bool toGenHomMat2D(CMvLineSeg InP,CMvLineSeg GetP);
    //
    bool toGenHomMat2D(QVector<CMvPoint> &InP,QVector<CMvPoint> &GetP);
    //围绕坐标0点完成比例缩放
    bool toGenHomMat2D(double sx,double sy);
    //
    //区域转换,是否正向传播
    bool toGetOutReg(HObject *treg,HObject &toutreg,bool isTran=true);
    //图像转换
    bool toGetOutImage(HObject *tInim,HObject &tOutIm,bool isTran=true);
    //点转换
    bool toGetOutPontF(CMvPoint &tInPoint,CMvPoint &tOutPoint,bool isTran=true);
//    //矩形变换
//    bool toGetOutRect(CMvRect &tInRect,CMvRect &tOutRect,bool isTran=true);
    bool toGetOutRect(CMvRect &tInRect,CMvRotatedRect &tOutRect,bool isTran=true);

    //旋转矩形变换
    bool toGetOutRotaRect(CMvRotatedRect &tInRect,CMvRotatedRect &tOutRect,bool isTran=true);
    //旋转线
    bool toGetOutLineF(CMvLineSeg &tInLine,CMvLineSeg &tOutLine,bool isTran=true);
    //旋转偏移点集
    bool toGetOutPointS(QVector<CMvPoint> &tInpoints,QVector<CMvPoint> &tOutpoints,bool isTran=true);
    //转移圆弧
    bool toGetOutCircleArc(CMvArc &inArc, CMvArc &outArc, bool isTran=true);
    //转移园
    bool toGetOutCircle(CMvCircle &inCircle, CMvCircle &outCircle, bool isTran=true);
    //转移数值
    bool toGetOutDouble(double SetDouble,double &GetDouble,bool isTan=true);

};


//通用投影变换
class Q_DECL_EXPORT YtHomtprojPro
{
    //坐标系统一类
public:
    YtHomtprojPro();
    ~YtHomtprojPro();
public:
    //因为只是一个处理类，所以就不存在模板这个概念
    HTuple m_HomMat2D;//转换对象,正向转换
    HTuple m_HomMat2DD;//反向转化，比如把图像反向修正
public:
    //
    bool toGenHomMat2D(QVector<CMvPoint> &InP,QVector<CMvPoint> &GetP);
    //
    //区域转换,是否正向传播
    bool toGetOutReg(HObject &treg,HObject &toutreg,bool isTran=true);
    //图像转换
    bool toGetOutImage(HObject &tInim,HObject &tOutIm,bool isTran=true);
    //点转换
    bool toGetOutPontF(CMvPoint &tInPoint,CMvPoint &tOutPoint,bool isTran=true);
    //旋转线
    bool toGetOutLineF(CMvLineSeg &tInLine,CMvLineSeg &tOutLine,bool isTran=true);
    //旋转偏移点集
    bool toGetOutPointS(QVector<CMvPoint> &tInpoints,QVector<CMvPoint> &tOutpoints,bool isTran=true);
    //转移园
    bool toGetOutCircle(CMvCircle &inCircle, CMvCircle &outCircle, bool isTran=true);
};
//找边缘点后续找线的算子设计
struct CabEdgePosPar
{
    double Sigma=1.0;//平滑系数
    int ThVal=20;
    int ThValType=0;//只能是 0 1 u8"梯度#阈值"
    int PosTans=0;//只能是 0 1 2 u8"所有#白到黑#黑到白"
    int PosGet=0;//u8"所有#最强点#起点#终点#起二点#末二点#起末点"
};
QString Q_DECL_EXPORT toCabGetProEdgePro(HObject &inim, CMvRotatedRect OutFindRect, CabEdgePosPar CabPar, QVector<CMvPoint> &GetAllPos,QVector<CMvPoint> &SelectPos);
int Q_DECL_EXPORT toGetMaxIndex(QVector<double> &SetVal);
int Q_DECL_EXPORT toGetMinIndex(QVector<double> &SetVal);
int Q_DECL_EXPORT YtCalclSiderPro(CMvLineSeg insegLine, double widthset, QVector<CMvRotatedRect> &OutRects, QVector<CMvLineSeg> &OutLines, int tSpitNumbe);//根据卡尺计算搜索
//线拟合算法
QString Q_DECL_EXPORT toFitLinePro(QVector<QVector<CMvPoint> > &InPoints, CMvLineSeg &OutLine, QVector<CMvPoint> &OutSearPos, int Searchmode=0, int itrorNum=10, int PDisTance=10);
//多线拟合算法,根据距离以及约束点数来进行拟合输出约束
QString Q_DECL_EXPORT toFitMultiLinePro(QVector<QVector<CMvPoint> > &InPoints, CMvLineSeg &SetLine, QVector<CMvLineSeg> &OutLines, int PointNumber=3,int PoseDistan=5);



int Q_DECL_EXPORT toGetLinesByCirArcPro(CMvArc &inCirArc, double widthset ,double hightset, QVector<CMvRotatedRect> &OutRects, QVector<CMvLineSeg> &outfindLine, int tSpitNumbe, int inFindtype=0);//根据卡尺计算搜索

//圆拟合算法
QString Q_DECL_EXPORT toFitCirclePro(QVector<QVector<CMvPoint>> &InPoints, CMvCircle &OutCircle, QVector<CMvPoint> &OutSearPos, int itrorNum=10, int PDisTance=10);





#endif // YTALGOBASEPRO_H
