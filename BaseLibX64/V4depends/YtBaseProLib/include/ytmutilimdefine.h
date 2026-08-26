#ifndef YTMUTILIMDEFINE_H
#define YTMUTILIMDEFINE_H
#include <QtGlobal>
#include <QPointF>
#include <QImage>
#include <QVector>
#include <QFile>
#include <QDataStream>
enum
{
  DefaltIniType=0,//缺损模式，至少
  FileInitType=1,//文件加载默认模式
  MemInitType=2,//序列化加载image模式
  MixSetType=3,//缩放小图与文件混合模式

};
class Q_DECL_EXPORT YtMutilImDefine
{
public:
    YtMutilImDefine();
    YtMutilImDefine(QSize SetSize,int WorkType,double setInnerscal=1.0);
    ~YtMutilImDefine();
public:
    YtMutilImDefine &  operator= (YtMutilImDefine &obj);
public:
    //这里要去拼接图象必须一样大 还是保留图象不一样的变化
    //核心问题就是喷绘起点
    //如果按照点的关系就没办法完成图象汇总裁切
    QVector<QImage> ShowIm;//
    QVector<QString> ImPathList;
    QVector<QPointF> ShowImPos;
    double InnerScal=1;//必然是一个小于等于1数
    QSize ShowSize;
    int SaveSetMode=DefaltIniType;
    QString SetReadFileName;
public:
    int width();
    int height();
    QImage::Format format();
    bool isempty();
    void toSetSaveMode(int SetMode);
    int toGetSaveMode();
    //设置内部尺寸
    void toSetSize(QSize tSetSize);
    QSize toGetSize();
    //大图裁切
    QImage toCopy(QRect tGetPos);
    //获取图象格式
    QImage::Format toGetformat();
public:
    //文件倍率
    void toClearInnerData();
    void toSetInnerScal(double setscal=1);
    double toGetInnerScal();
    //
    void toAppendFileImage(QString fileName,QPointF SetPos);
    //图象文件
    void toAppendQImage(QImage SetIm,QPointF SetPos);
public:
    //正反序列化
    void toSaveFile(QString SetFileName);
    void toLoadFile(QString SetFileName);
};

#endif // YTMUTILIMDEFINE_H
