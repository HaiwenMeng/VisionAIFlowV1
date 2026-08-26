#ifndef YTVISIONOBJECT_H
#define YTVISIONOBJECT_H
#include <QImage>
#include "YtVisionBlob.h"
#include "ytvisionbasefun.h"
#include "YtVisionOverPlay.h"
//对QImage进行拓展,使得能够支持内存挂载方便中途传输
//使用Qimage的浅拷贝就可以实现内存共享
class Q_DECL_EXPORT YtInterIm
{
    //此对象用于构建算法执行dll的中间承载
    //存储中间共享内存的变量，方便传输使用
private:
    uchar * data;
    int width;
    int height;
    int bytesPerLine;
    int format;//记录中间承载，
    QImage mIM;//内部承载
public:
    YtInterIm(uchar * data=nullptr, int width=400, int height=400, int bytesPerLine=400, int format=QImage::Format_Grayscale8);
    void toSetQimamage(QImage im);//承载图像，浅拷贝
    void toSetCloenImage(QImage im);//承载图像，深拷贝
    QImage toGetIm();//拿出对象
    void toSetData(uchar * data=nullptr, int width=400, int height=400, int bytesPerLine=400, int format=QImage::Format_Grayscale8);
public:
    //基础调用方法
    bool LoadFile(QString filename);
    bool SaveFile(const QString &fileName, const char *format = Q_NULLPTR, int quality=-1);
    //获取图像指针
    unsigned char * GetImageData(unsigned int nX = 0, unsigned int nY = 0);
    //按照单通道读取灰度值
    int GetGrayVal(unsigned int nX = 0, unsigned int nY = 0);
    //获取图像宽度
    int GetImageWidth();
    //获取图像高度
    int GetImageHight();
    //获取图像类型
    int GetImageType();
};
//增加一些基本数据类型的构造,与Qt的数据类型相互转换
QPointF  Q_DECL_EXPORT YtMVPointToQpoint(CMvPoint tempoint);
CMvPoint Q_DECL_EXPORT YtQpointToMvPoint(QPointF tempoint);
//线段转换
CMvLineSeg Q_DECL_EXPORT YtQlineToMvLine(QLineF temline);//直线构造转换
double Q_DECL_EXPORT YtMvLineSegAng(CMvLineSeg temline);//直线角度计算
double Q_DECL_EXPORT YtMvLineSegLenth(CMvLineSeg temline);//直线长度计算
CMvPoint Q_DECL_EXPORT YtMvLineSegCenter(CMvLineSeg temline);//获取直线的中点
//正矩形
CMvRect Q_DECL_EXPORT YtQRectToMvRect(QRectF temrect);
double Q_DECL_EXPORT YtMvRectWidth(CMvRect temrect);//获取宽度
double Q_DECL_EXPORT YtMvRectHeight(CMvRect temrect);//获取高度
CMvPoint Q_DECL_EXPORT YtMvRectCenter(CMvRect temrect);//正矩形的中心
//旋转矩形的相关配置
CMvRotatedRect Q_DECL_EXPORT YtCreateRoTatedRect(double cx = 0, double cy = 0, double w = 0, double h = 0, double angle = 0);//构建旋转矩形
CMvRotatedRect Q_DECL_EXPORT YtCreateRoTatedRect( QPointF center,QSizeF size, double angle);//构建旋转矩形
//  pts[0] ------------ pts[1]
//         |          |
//         |          |
//         |    -->   O  箭头方向为正方向. 角度是逆时针为正(与Qt/Halcon方向一致, 与OpenCv方向相反)
//         |          |
//         |          |
//  pts[3] ------------ pts[2]
int Q_DECL_EXPORT YtRotatedRectPoints(CMvRotatedRect &inrect,QPointF (&GetPoints)[4]);
int Q_DECL_EXPORT YtRotatedRectPoints(CMvRotatedRect &inrect,CMvPoint (&GetPoints)[4]);
//获取旋转矩形外接正矩形
QRectF Q_DECL_EXPORT YtGetRotatedRectMaxRect(CMvRotatedRect &inrect);
CMvRect Q_DECL_EXPORT YtGetRotatedRectMaxMRect(CMvRotatedRect &inrect);
//增加读写 overplay 接口  后追 overlay
void Q_DECL_EXPORT CCommonSaveOverlayData(YtSetShowtObj &inOverlay, QString strFileName);
bool Q_DECL_EXPORT CCommonLoadOverlayData(YtSetShowtObj &outOverlay, QString strFileName);



#endif // YTVISIONOBJECT_H
