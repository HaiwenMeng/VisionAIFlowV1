#ifndef YTVISIONBLOB_H
#define YTVISIONBLOB_H
#include "ytvisiondefine.h"
#include <QFile>
#include <QDataStream>
struct YtBlobDefine
{
    QString Type;        // 类型32个字符表示
    CMvRotatedRect Pos; // blob位置信息
    CMvRect  RectPos;   // 正矩形位置
    double Len1;        // 实际长轴长
    double Len2;        // 实际短轴长
    double Area;        // 缺陷面积
    double Compact;     // 紧实度
    double Reserve[4];  // 保留数据
    friend QDataStream&operator >>(QDataStream &stream,YtBlobDefine &SetData)
    {
        stream>>SetData.Type>>SetData.Pos>>SetData.RectPos>>SetData.Len1>>SetData.Len2>>SetData.Area>>SetData.Compact
                >>SetData.Reserve[0]>>SetData.Reserve[1]>>SetData.Reserve[2]>>SetData.Reserve[3];
        return stream;
    }
    //输出
    friend QDataStream&operator <<(QDataStream &stream,const YtBlobDefine &SetData)
    {
        stream<<SetData.Type<<SetData.Pos<<SetData.RectPos<<SetData.Len1\
             <<SetData.Len2<<SetData.Area<<SetData.Compact<<SetData.Reserve[0]\
            <<SetData.Reserve[1]<<SetData.Reserve[2]<<SetData.Reserve[3];
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const YtBlobDefine &SetData)
    {

        debug<<QString("Blob(%1,%2x%3)").arg(SetData.Type).arg(SetData.Len1).arg(SetData.Len2);
        return debug;
    }
};


struct YtAllBlob
{
    double Xscal=1.0;//记录X方向比例尺
    double Yscal=1.0;//记录Y比例尺信息
    CMvPoint GetPos[4];//四点定位的包络
    QVector<QString>    GetImInfo;//输出图像信息
    QVector<YtBlobDefine> GetBlob;//记录所有的blob信息
    friend QDataStream&operator >>(QDataStream &stream,YtAllBlob &SetData)
    {
        stream>>SetData.Xscal>>SetData.Yscal>>SetData.GetPos[0]>>SetData.GetPos[1]>>SetData.GetPos[2]>>SetData.GetPos[3]>>SetData.GetImInfo>>SetData.GetBlob;
        return stream;
    }
    //输出
    friend QDataStream&operator <<(QDataStream &stream,const YtAllBlob &SetData)
    {
        stream<<SetData.Xscal<<SetData.Yscal<<SetData.GetPos[0]<<SetData.GetPos[1]<<SetData.GetPos[2]<<SetData.GetPos[3]<<SetData.GetImInfo<<SetData.GetBlob;
        return stream;
    }
    friend QDebug &operator <<(QDebug debug,const YtAllBlob &SetData)
    {

        debug<<QString("AllBlob(%1,%2x%3)").arg(SetData.GetBlob.size());
        for(int i=0;i<SetData.GetBlob.size();i++)
        {
            debug<<QString("Blob(%1,%2x%3)").arg(SetData.GetBlob.at(i).Type).arg(SetData.GetBlob.at(i).Len1).
                   arg(SetData.GetBlob.at(i).Len2);
        }
        return debug;
    }

    void toSaveData(QString filename)
    {
        QFile file(filename);
        if(!file.open(QIODevice::WriteOnly))
        {
            return;
        }
        QDataStream out(&file);
        out<<Xscal<<Yscal<<GetPos[0]<<GetPos[1]<<GetPos[2]<<GetPos[3]<<GetImInfo<<GetBlob;
        file.close();
    }
    void toLoadData(QString filename)
    {
        QFile file(filename);
        if(!file.open(QIODevice::ReadOnly))
        {
            return;
        }
        QDataStream out(&file);
        out>>Xscal>>Yscal>>GetPos[0]>>GetPos[1]>>GetPos[2]>>GetPos[3]>>GetImInfo>>GetBlob;
        file.close();
    }

};


#endif // YTVISIONBLOB_H
