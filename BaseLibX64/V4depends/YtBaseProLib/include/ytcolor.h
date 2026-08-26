#ifndef YtQColor_H
#define YtQColor_H

#include "YtTypeBaseDefinex.h"

class Q_DECL_EXPORT YtQColor : public YtVarBassX
{
public:
    //有link表示符号的info就会起到索引作用，方便传递数值用的很重要
    YtQColor(QColor bValue = QColor(), QString strVarInfo = "",int DataType=YtVal);
    ~YtQColor();
public:
    QByteArray toData();                                                 // 序列化写入数据
    void toGetData(QByteArray &SetData);
    QString toGetDebugData();//遍历信息
    YtVarBassX *toCopy();//自己的深拷贝
    QColor toQColor();
    void toSetQColor(QColor bValue);
    void toShutShoot();//快照内部数据
    void toReSetData();//将快照数据写入
    bool toGetChange(QString &info);//记录变化
    QStringList toGetInerSet();


private:
    QColor m_Value;//当前值
    QColor m_ValueShoot;//快照数值
};
#endif // YtQColor_H
