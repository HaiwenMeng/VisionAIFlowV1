#ifndef YtVarBassX_H
#define YtVarBassX_H
#include "ytvisiondefine.h"
#include <QMap>
#include <QFont>
#include <QMutex>
#include <QMutexLocker>
#include <QtCore>

enum YTVarType
{
    YBool = 1,
    YInt,
    YDouble,
    YQString,
    YQStringList,
    YQColor,
    YQFont,
    YQTime,
    YQDate,
    YQDateTime,
    YVoidPtr,//这个作为指针输入输出的定义使用，一般也用不上
    YCMvPoint,
    YCMvLineSeg,
    YCMvCircle,
    YCMvSize,
    YCMvRect,
    YCMvRotatedRect,
    YCMvPolygon,
    YCMvArc,
    YCMvEllipse,
    YSelfDefinPtr,//自定义类型，在各自使用的地方进行定义
};

enum YtDataType
{
    YtVal=1,//数值类型
    YtEnum,//枚举类型
    YtTip,//指示只对qstring有效
    YtFile,//文件注释
    YtPath,//路径
    YtIp,//ip格式
    YtLinkVal,//数值传递类型，可以是任意类型
};

//确定使用序列化技术，后期只要能够读出来的专用软件即可，没必要折腾了
class Q_DECL_EXPORT YtVarBassX
{
public:
    YtVarBassX() {}
    ~YtVarBassX() {}
public:
    virtual QByteArray toData() = NULL;                                                 // 序列化写入数据
    virtual void toGetData(QByteArray &SetData) = NULL;                                 // 序列化读取数据
    //
    virtual void toSetKeyName(QString KeyName) {m_KeyName = KeyName;}   // 设置类名称
    virtual QString toGetKeyName() {return m_KeyName;}                            // 获取类名称
    //
    virtual QString toGetParentName() {return m_ParentName;}
    virtual void toSetParentName(QString ParentName) {m_ParentName=ParentName;}
    //设置父类名称，因为现在都是使用指针管理，直接拿插件里面的改变父的值就行，针对link属性的，可以双击查看下数值
    virtual int toGetVarType() {return m_VarType;}                                              // 获取数据类型
    virtual void toSetVarType(int VarType) {m_VarType=VarType;}
    //
    virtual int toGetDataType() {return m_DataType;}                                    //获取数据定义格式，我们只对特定的格式进行控制
    virtual void toSetDataType(int type) {m_DataType=type;}
    //
    virtual void toSetPtrClass(QString strClassName) {m_strClassName = strClassName;}   // 设置类名称
    virtual QString toGetPtrClass() {return m_strClassName;}                            // 获取类名称
    //
    virtual void toSetInfoData(QString varInfo) {m_strVarInfo = varInfo;}               // 设置备注信息
    virtual QString toGetInfoData() {return m_strVarInfo;}                              // 获取备注信息
    //
    virtual QString toGetDebugData() {return "";}                                       // 遍历打印内部调试信息
    //
    virtual YtVarBassX *toCopy(){return nullptr;}
    //
    virtual bool toBool() {return false;}
    virtual void toSetBool(bool SetVal) {}
    //
    virtual int toInt() {return 0;}
    virtual void toSetInt(int SetVal) {}
    //
    virtual double toDouble() {return 0;}
    virtual void toSetDouble(double SetVal) {}
    //
    virtual QString toQString() {return 0;}
    virtual void toSetQString(QString SetVal) {}
    //
    virtual QStringList toQStringList() {return  QStringList();}
    virtual void toSetQStringList(QStringList SetVal) {}
    //
    virtual QColor toQColor() {return QColor(0,0,0);}
    virtual void toSetQColor(QColor SetVal) {}
    //
    virtual QFont toQFont() {return QFont();}
    virtual void toSetQFont(QFont SetVal) {}
    //
    virtual QTime toQTime() {return QTime();}
    virtual void toSetQTime(QTime SetVal) {}
    //
    virtual QDate toQDate() {return QDate();}
    virtual void toSetQDate(QDate SetVal) {}
    //
    virtual QDateTime toQDateTime() {return QDateTime();}
    virtual void toSetQDateTime(QDateTime SetVal) {}
    //
    virtual CMvPoint toCMvPoint() {return CMvPoint();}
    virtual void toSetCMvPoint(CMvPoint SetVal) {}
    //
    virtual CMvLineSeg toCMvLineSeg() {return CMvLineSeg();}
    virtual void toSetCMvLineSeg(CMvLineSeg SetVal) {}
    //
    virtual CMvCircle toCMvCircle() {return CMvCircle();}
    virtual void toSetCMvCircle(CMvCircle SetVal) {}
    //
    virtual CMvSize toCMvSize() {return CMvSize();}
    virtual void toSetCMvSize(CMvSize SetVal) {}
    //
    virtual CMvRect toCMvRect() {return CMvRect();}
    virtual void toSetCMvRect(CMvRect SetVal) {}
    //
    virtual CMvRotatedRect toCMvRotatedRect() {return CMvRotatedRect();}
    virtual void toSetCMvRotatedRect(CMvRotatedRect SetVal) {}
    //
    virtual CMvPolygon toCMvPolygon() {return CMvPolygon();}
    virtual void toSetCMvPolygon(CMvPolygon SetVal) {}
    //
    virtual CMvArc toCMvArc() {return CMvArc();}
    virtual void toSetCMvArc(CMvArc SetVal) {}
    //
    virtual CMvEllipse toCMvEllipse() {return CMvEllipse();}
    virtual void toSetCMvEllipse(CMvEllipse SetVal) {}
    //
    virtual void* toVoidPtr() {return nullptr;}
    virtual void toSetVoidPtr(void* SetVal) {}
    //
    virtual void* toSelfDefinPtr() {return nullptr;}
    virtual void toSetSelfDefinPtr(void* SetVal) {}
    //
    virtual void toSetVal(YtVarBassX *Par) {}
    //
    virtual void toShutShoot(){}//快照内部数据
    virtual void toReSetData(){}//将快照数据写入
    virtual bool toGetChange(QString &info){info.clear();return false;}//记录变化
    //
    virtual QStringList toGetInerSet() {return QStringList();}

private:
    QString m_KeyName;//注册名称
    QString m_strVarInfo;   // 注释信息
    QString m_strClassName; // 管理的类名称
    int m_DataType;//数据模式
    int m_VarType;//数据类型
    QString m_ParentName;//父名称
};

#endif // YtVarBassX_H
