#ifndef YTPARVARXLIB_H
#define YTPARVARXLIB_H
#include <QtCore/qglobal.h>
#include <QList>
#include <QMap>
#include "ytboolvar.h"
#include "ytintvar.h"
#include "ytdouble.h"
#include "ytstring.h"
#include "ytstringlist.h"
#include "ytcolor.h"
#include "ytfont.h"
#include "yttime.h"
#include "ytdate.h"
#include "ytdatetime.h"
#include "ytvoidptr.h"
#include "ytcmvpoint.h"
#include "ytcmvlineseg.h"
#include "ytcmvcircle.h"
#include "ytcmvsize.h"
#include "ytcmvrect.h"
#include "ytcmvrotaterect.h"
#include "ytcmvpolygon.h"
#include "ytcmvarc.h"
#include "ytcmvellipse.h"
class YtVarBassX;
class Q_DECL_EXPORT YtVarSheetX
{
public:
    YtVarSheetX();
    ~YtVarSheetX();

private:
    QMap<QString, YtVarBassX*> m_mapInnerData;
    QStringList  m_listVarKey;
public:
    //从文件载入,为了方便多个var把数据合成一个文件，需要一个自定义头，没有忽略
    bool toLoadData(QString strFileName);
    bool toSaveData(QString strFileName);//保存到文件
    QByteArray toData();//序列化数据
    void toGetData(QByteArray &Data);//根据二进制数据解析内部存储
    QList<QString> toGetKeyList();//获取内部插入列表
    YtVarBassX *toGetKeyVal(QString strVarKey);//获得基类指针，根据基类指针去访问类型
    bool toInsertData(YtVarBassX *pVar, QString strVarKey);//插入数据指针
    bool toRemoveData(QString strVarKey,bool isclear=false);
    void toClearData(bool bClearPtr = false);//是否析构指针的处理
    QStringList toGetDebugData();
    void toCopyFrom(YtVarSheetX &varSheet, bool isclear=false);//深拷贝
public:
     void toShutShoot();//快照内部数据
     void toReSetData();//将快照数据写入
     QStringList toGetChange();//记录变化
};
//从列表中搜去信息
bool Q_DECL_EXPORT toUpdateValueFromSheet(YtVarBassX* SetPar,YtVarSheetX *SetSheet);
#endif // YTPARVARXLIB_H
