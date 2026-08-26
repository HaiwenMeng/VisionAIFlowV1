#ifndef YtBool_H
#define YtBool_H

#include "YtTypeBaseDefinex.h"

class Q_DECL_EXPORT YtBool : public YtVarBassX
{
public:
    //有link表示符号的info就会起到索引作用，方便传递数值用的很重要
    YtBool(bool bValue = false, QString strVarInfo = "NULL",int DataType=YtVal);
    ~YtBool();
public:
    QByteArray toData();                                                 // 序列化写入数据
    void toGetData(QByteArray &SetData);
    QString toGetDebugData();//遍历信息
    YtVarBassX *toCopy();//自己的深拷贝
    bool toBool();
    void toSetBool(bool bValue);
    void toShutShoot();//快照内部数据
    void toReSetData();//将快照数据写入
    bool toGetChange(QString &info);//记录变化
    QStringList toGetInerSet();

private:
    bool m_Value;//当前值
    bool m_ValueShoot;//快照数值
};
#endif // YtBool_H
