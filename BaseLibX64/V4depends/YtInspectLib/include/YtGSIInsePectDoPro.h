#ifndef YTGSIINSEPECTDOPRO_H
#define YTGSIINSEPECTDOPRO_H
#include <QtGlobal>
class YtInPDoInnerI;
#include <QFile>
#include <QDataStream>
#include "YtVisionOverPlay.h"
//点定义
struct NCMvPoint
{
    double x=0.0;//点的x轴坐标，默认值为0.0
    double y=0.0;//点的y轴坐标，默认值为0.0
};
//旋转矩形定义
struct NCMvRotatedRect
{
    NCMvPoint  Center;//中心点位置，默认值为(0.0, 0.0)
    double  cx=0.0 ;//x轴方向宽度，默认值为0.0
    double  cy=0.0 ;//y轴方向宽度，默认值为0.0
    double angle=0.0;//旋转角度，弧度
};
//正矩形定义
struct NCMvRect
{
    NCMvPoint  LeftTop;//左上点位置，默认值为(0.0, 0.0)
    double  cx=0.0 ;//x轴方向宽度，默认值为0.0
    double  cy=0.0 ;//y轴方向宽度，默认值为0.0
};
struct NYtBlobDefine
{
    char Type[32];      // 类型32个字符表示
    NCMvRotatedRect Pos; // blob位置信息
    NCMvRect  RectPos;   // 正矩形位置
    double Len1;        // 实际长轴长
    double Len2;        // 实际短轴长
    double Area;        // 缺陷面积
    double Compact;     // 紧实度
    double Reserve[4];  // 保留数据
};
struct NYtAllBlob
{
    double Xscal=1.0;//记录X方向比例尺
    double Yscal=1.0;//记录Y比例尺信息
    int OutTypeDefine;//输出最终定义,有可能就是true 和false
    NCMvPoint m_GetPos[4];//四点定位的包络
    QVector<QString>    GetImInfo;//输出图像信息
    QVector<NYtBlobDefine> GetBlob;//记录所有的blob信息
    ////////////////
    QString ProInimInfo;//特定信息
    void toScalBlob(NYtAllBlob &FromData,double scalfactor=0.25)
    {
        Xscal=FromData.Xscal;
        Yscal=FromData.Yscal;
        OutTypeDefine=FromData.OutTypeDefine;
        ProInimInfo=FromData.ProInimInfo;
        GetImInfo=FromData.GetImInfo;
        for(int i=0;i<4;i++)
        {
            m_GetPos[i].x=FromData.m_GetPos[i].x*scalfactor;
            m_GetPos[i].y=FromData.m_GetPos[i].y*scalfactor;

        }
        GetBlob.clear();
        GetBlob.append(FromData.GetBlob);
        for (int i = 0; i < GetBlob.size(); i++)
        {
            GetBlob[i].Area*=scalfactor*scalfactor;
            GetBlob[i].Len1*=scalfactor;
            GetBlob[i].Len2*=scalfactor;
            GetBlob[i].RectPos.LeftTop.x*=scalfactor;
            GetBlob[i].RectPos.LeftTop.y*=scalfactor;
            GetBlob[i].RectPos.cx*=scalfactor;
            GetBlob[i].RectPos.cy*=scalfactor;
            GetBlob[i].Pos.Center.x*=scalfactor;
            GetBlob[i].Pos.Center.y*=scalfactor;
            GetBlob[i].Pos.cx*=scalfactor;
            GetBlob[i].Pos.cy*=scalfactor;
        }

    }
    QByteArray toStreamData()
    {
        QByteArray OutData;
        QDataStream out(&OutData,QIODevice::ReadWrite);
        out << Xscal<<Yscal<<OutTypeDefine<<GetImInfo<<GetBlob.size();
        QByteArray temByteData;
        NYtBlobDefine temsetdata;

        for(int i=0;i<GetBlob.size();i++)
        {
            temsetdata=GetBlob.at(i);
            temByteData.setRawData((char *)&temsetdata,sizeof(NYtBlobDefine));
            out<<temByteData;
        }
        out<<ProInimInfo;

        out<<m_GetPos[0].x<<m_GetPos[0].y;
        out<<m_GetPos[1].x<<m_GetPos[1].y;
        out<<m_GetPos[2].x<<m_GetPos[2].y;
        out<<m_GetPos[3].x<<m_GetPos[3].y;

        return  OutData;
    }
    //反序列化blob数据
    void toFromStreamData(QByteArray &Indata)
    {
        int SetSize=0;
        GetBlob.clear();
        QDataStream out(&Indata,QIODevice::ReadWrite);
        out >> Xscal>>Yscal>>OutTypeDefine>>GetImInfo>>SetSize;
        QByteArray temByteData;
        NYtBlobDefine temsetdata;
        for(int i=0;i<SetSize;i++)
        {
            out>>temByteData;
            memcpy(&temsetdata,temByteData.data(),temByteData.size());
            GetBlob.append(temsetdata);
        }
        out>>ProInimInfo;

        out>>m_GetPos[0].x>>m_GetPos[0].y;
        out>>m_GetPos[1].x>>m_GetPos[1].y;
        out>>m_GetPos[2].x>>m_GetPos[2].y;
        out>>m_GetPos[3].x>>m_GetPos[3].y;


    }
    void toSaveData(QString filename)
    {
        QFile file(filename);
        if(!file.open(QIODevice::WriteOnly))
        {
            return;
        }
        QDataStream out(&file);
        out << Xscal<<Yscal<<OutTypeDefine<<GetImInfo<<GetBlob.size();
        QByteArray temByteData;
        NYtBlobDefine temsetdata;
        for(int i=0;i<GetBlob.size();i++)
        {
            temsetdata=GetBlob.at(i);
            temByteData.setRawData((char *)&temsetdata,sizeof(NYtBlobDefine));
            out<<temByteData;
        }
        out<<ProInimInfo;

        out<<m_GetPos[0].x<<m_GetPos[0].y;
        out<<m_GetPos[1].x<<m_GetPos[1].y;
        out<<m_GetPos[2].x<<m_GetPos[2].y;
        out<<m_GetPos[3].x<<m_GetPos[3].y;
        file.close();
    }
    void toLoadData(QString filename)
    {
        QFile file(filename);
        if(!file.open(QIODevice::ReadOnly))
        {
            return;
        }
        int SetSize=0;
        GetBlob.clear();
        QDataStream out(&file);
        out >> Xscal>>Yscal>>OutTypeDefine>>GetImInfo>>SetSize;
        QByteArray temByteData;
        NYtBlobDefine temsetdata;
        for(int i=0;i<SetSize;i++)
        {
            out>>temByteData;
            memcpy(&temsetdata,temByteData.data(),temByteData.size());
            GetBlob.append(temsetdata);
        }
        out>>ProInimInfo;

        out>>m_GetPos[0].x>>m_GetPos[0].y;
        out>>m_GetPos[1].x>>m_GetPos[1].y;
        out>>m_GetPos[2].x>>m_GetPos[2].y;
        out>>m_GetPos[3].x>>m_GetPos[3].y;

        file.close();
    }

};

//读取图像接口
bool Q_DECL_EXPORT toNLoadQImage(QImage &OutIm,QString FileName);
//写入图像接口
bool Q_DECL_EXPORT toNSaveQImage(QImage &InIm, QString FileNmae, int Perset=80);
class Q_DECL_EXPORT YtGSIInsePectDoPro
{
public:
    YtGSIInsePectDoPro();
    ~YtGSIInsePectDoPro();
private:
    YtInPDoInnerI *m_YtInPDoInner=nullptr;
public:
    bool toLoadMode(QString Path);//载入模板
    bool toSaveMode();//保存模板
    void toSetPar();//参数设置弹窗
    bool toExecIm(QImage InIm, YtGetRestObj *OutData);//直接执行如果返回false说明内部流程失败，true说明执行成功
    bool toExecMultiIm(QVector<QImage> &Inim);//检测多张图像
    QList<QString> toGetErrPlugininfo();//将错误的插件注释信息输出,便于查找问题
    bool toGetDetcBlob(NYtAllBlob &OutBlob,QString &OutStr);//执行请求,产品错误集合
    void toSetLogeName(QString strlogname);
};

#endif // YTGSIINSEPECTDOPRO_H
