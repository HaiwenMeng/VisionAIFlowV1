#ifndef YtCMvCircle_H
#define YtCMvCircle_H

#include "YtTypeBaseDefinex.h"
class Q_DECL_EXPORT YtCMvCircle : public YtVarBassX
{
public:
    //有link表示符号的info就会起到索引作用，方便传递数值用的很重要
    YtCMvCircle(CMvCircle bValue=CMvCircle(), QString strVarInfo = "",int DataType=YtVal);
    ~YtCMvCircle();
public:
    QByteArray toData();                                                 // 序列化写入数据
    void toGetData(QByteArray &SetData);
    QString toGetDebugData();//遍历信息
    YtVarBassX *toCopy();//自己的深拷贝
    CMvCircle toCMvCircle();
    void toSetCMvCircle(CMvCircle bValue);
    void toShutShoot();//快照内部数据
    void toReSetData();//将快照数据写入
    bool toGetChange(QString &info);//记录变化
    QStringList toGetInerSet();
private:
    CMvCircle m_Value;//当前值
    CMvCircle m_ValueShoot;//快照数值
};
#endif // YtCMvCircle_H
