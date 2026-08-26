#ifndef YTGSIINSPECTV5_H
#define YTGSIINSPECTV5_H
#include "YtGSIInsePectDoPro.h"
#include "YtWidProVirtualClass.h"
#include "YtVisionOverPlay.h"
#include <QtGlobal>
class YtInPDoInnerI;
#include <QFile>
#include <QDataStream>
#include "ytparvarxlib.h"
#include "YtOutListDataDefine.h"


void Q_DECL_EXPORT toDrawBlob(NYtAllBlob &SetBlob,YtSetShowtObj &outBlob,bool isclear=true);
void Q_DECL_EXPORT toResetBlob(NYtAllBlob &SetBlob,int setx,int sety);

void Q_DECL_EXPORT toCutBlobIm(QImage SetIm,NYtAllBlob &SetBlob,YtSetShowtObj &outBlob,QString SavePath="",int Imagesize=400,int Maxim=5);

void Q_DECL_EXPORT toGenRegionCircleshow(void *Himptr, YtSetShowtObj &outBlob, int Maxim=50);

QString Q_DECL_EXPORT toGetWorkNameSet();
class Q_DECL_EXPORT YtGSITipFile
{
public:
    YtGSITipFile();
public:
    int m_GetRest;//0 未完成 1 OK 2NG
public:
    //是否没有就创建
    int toGetResType(QString filename);//获取结果
    //保存结果
    void toSaveFileName(QString filename,int type);
    void toCreateId(QString Path,QString Id,QString WorkName="CAA#CAB");
    //请求一定是最新的idresult文件夹路径，自己根据名称去存就行
    bool toGetId(QString Path,QString Id,QString &OutPath);

};


class Q_DECL_EXPORT YtGsiInspectV5
{
public:
    YtGsiInspectV5();
    ~YtGsiInspectV5();
private:
    YtInPDoInnerI *m_YtInPDoInner=nullptr;
public:
    bool toLoadMode(QString Path);//载入模板
    bool toSaveMode();//保存模板
    void toSetPar();//参数设置弹窗
    void toSetLogeName(QString strlogname);//
public:
    //内部帮忙把图层算好
    bool toExecIm(QImage InIm, YtGetRestObj *OutData);
    bool toExecIm(QImage InIm, YtGetRestObj *OutData, bool SaveInputHImage);//直接执行如果返回false说明内部流程失败，true说明执行成功
    //通用检测接口
    bool toExecPro(QList<PtrDefine> SetPtr);
    QStringList toGetInnerIm();//内部图象索引
    QImage toGetIm(QString Key);
    QString toGetErrMesg();
    bool toTakeSaveInputHImage(void *OutHImage);
public:
    bool toGetDetcBlob(NYtAllBlob &OutBlob, OutDataShow &OutData);//执行请求,产品错误集合
    void toGetOverPlayShow(YtSetShowtObj &OutOverPlay);
public:
    void toShowParView(QString ParamPath);//显示点检参数

};

#endif // YTGSIINSPECTV5_H
