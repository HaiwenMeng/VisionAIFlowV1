#ifndef YTROISHOWDISP_H
#define YTROISHOWDISP_H

#include <QWidget>
#include <QtUiPlugin/QDesignerExportWidget>
enum ROIShape {
    circleROI = 0,      //圆形ROI
    ellipseROI = 1,     //椭圆ROI
    rectangleROI = 2,   //矩形ROI
    rotaterectangleROI = 3,//旋转矩形ROI
    lineSegCabROI = 4,        //直线ROI卡尺
    arcROI = 5,         //弧形ROI
    ringROI = 6,        //圆环ROI
    polygonROI = 7,     //多边形ROI
    waistROI = 8,       //腰形ROI
    pieROI = 9,          //扇形ROI
    pointROI = 10 ,       //点ROI
    polygonROIE = 11,     //多边ROI,非循环点集
    lineSegROI = 12,        //线段

};
namespace Ui {
class YtRoiShowDisp;
}

class QDESIGNER_WIDGET_EXPORT YtRoiShowDisp : public QWidget
{
    Q_OBJECT

public:
    explicit YtRoiShowDisp(QWidget *parent = nullptr);
    ~YtRoiShowDisp();

private:
    Ui::YtRoiShowDisp *ui;
public:
     //固定绘制 体系
     void addStdOverPlayPtr(void* tpart,QString key="test");
     void removeStdOverPlayPtr(QString key="test");
     void ClearAllStdOverPlayPtr();
     //图像视图绘制
     void addOverPlayPtr(void* tpart,QString key="test");
     void removeOverPlayPtr(QString key="test");
     void ClearAllOverPlayPtr();
     //
     void toViewPoint( double centerx=0, double centery=0,double scale=1.0);//按照缩放倍数把显示点拉到中心
public:
     void toSetBGColor(QColor color);//设置背景色
     void toSetImage(QImage showim);//
     void toSetImageData(unsigned char *imagedata,int imwidth,int imhieht,int chanle=1);//是否为常驻内存，是的话避免内存多开
     void toLoadFileImage(QString filename);//文件名载入图像
     void toUpdateShow(bool isfitshow=false);//刷新显示
     void toSetLeftDoubleMode(int t=0,QString key="1");//0 为正常模式，
signals:
    void toGetPixInfo(QPoint Pos, QRgb rgb,int type);//获取灰度值响应
    void toLeftDoubleOut(QString key);
    void toRightClick(QString key);
    void ROIChange(QVector<double> tdata,QString &key,int type);
public:
    //ROI 体系
     void addROI(int shape, QVector<double> Val, QString key="Test");//增加ROI
     QVector<double> getROI(QString key="Test");//获取ROI
     void toRemoveRoiByKey(QString key="Test");//斜矩形，圆弧
     void toRemoveAllRoi();//清除所有的ROI
public:
     //为了方便调用内部构建了一个图层，外面可以直接显示管理
     void toClearInnerOverPlay();//清除内部显示
     void toClearInnerStdOverPlay();//清除内部静态显示
     void toApendInnerOverPlay(void *StdObj);//增加显示
     void toApendInnerStdOverPlay(void *StdObj);
     void toSetLineShow(int row=1,int col=1,int linewidth=1,QColor LineColor=Qt::blue);//显示线几行几列的
     QRect toGetViewIm();//获取当前图像显示位置
     QImage toGetShotCut();//获取当前widget的截图
     void toSetSliderView(bool istrue);//设置侧边查看
     QImage toGetImPaint(double GetScal=1.0);
};

#endif // YTROISHOWDISP_H
