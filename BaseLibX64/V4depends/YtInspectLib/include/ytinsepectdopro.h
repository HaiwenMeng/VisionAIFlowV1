#ifndef YTINSEPECTDOPRO_H
#define YTINSEPECTDOPRO_H
#include <QtGlobal>
#include "YtVisionOverPlay.h"
#include "ytcamerado.h"
#include "YtWidProVirtualClass.h"
class YtCameraDo;
class YtInPDoInnerI;
class YtVarSheetX;

enum EXEC_TYPE{
    Type_ExecInspect = 0,
    Type_ExecInspectX,
    Type_ExecPro
};

class Q_DECL_EXPORT YtInsePectDoPro
{
public:
    YtInsePectDoPro();
    ~YtInsePectDoPro();
public:
    //设置模板位置
    void toSetModelPath( QString model_path);
    //参数设置弹窗
    void toSetPar(YtCameraDo *Camr=nullptr, int nExecType = 0);
    //执行函数接口
    bool toExecInspect(QImage &Inim, YtGetRestObj *OutData= nullptr, YtSetShowtObj *overPlayObj = nullptr);
    //设置单图检测的设置空间,为了避免有多个小图设置
    void toSetSpecilDlg(QImage *MainPtr,int id=-1);
    //设置日志名称
    void toSetLogName(QString LogeName=u8"系统日志");
public:
    //新的调试接口,可以介入包括对比数值
    bool toExecInspectX(QImage &Inim, YtVarSheetX *OutData= nullptr, YtSetShowtObj *overPlayObj = nullptr);
    bool toExecPro(QList<PtrDefine> SetPtr, YtGetRestObj *OutData= nullptr, YtSetShowtObj *overPlayObj = nullptr);
private:
    YtInPDoInnerI *m_YtInPDoInner;
};

#endif // YTINSEPECTDOPRO_H
