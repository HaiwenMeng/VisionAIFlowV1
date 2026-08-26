#ifndef YtSuperShowDisp_H
#define YtSuperShowDisp_H
#include "ytmutilimdefine.h"
#include <QWidget>
#include <QtUiPlugin/QDesignerExportWidget>
enum SROIShape {
    ScircleROI = 0,      //圆形ROI
    SellipseROI = 1,     //椭圆ROI
    SrectangleROI = 2,   //矩形ROI
    SrotaterectangleROI = 3,//旋转矩形ROI
    SlineSegCabROI = 4,        //直线ROI卡尺
    SarcROI = 5,         //弧形ROI
    SringROI = 6,        //圆环ROI
    SpolygonROI = 7,     //多边形ROI
    SwaistROI = 8,       //腰形ROI
    SpieROI = 9,          //扇形ROI
    SpointROI = 10 ,       //点ROI
    SpolygonROIE = 11,     //多边ROI,非循环点集
    SlineSegROI = 12,        //线段

};
namespace Ui {
class YtSuperShowDisp;
}

class QDESIGNER_WIDGET_EXPORT YtSuperShowDisp : public QWidget
{
    Q_OBJECT

public:
    explicit YtSuperShowDisp(QWidget *parent = nullptr);
    ~YtSuperShowDisp();

private:
    Ui::YtSuperShowDisp *ui;
public:
     //固定绘制 体系
     void addStdOverPlayPtr(void* tpart,QString key="test");
     void removeStdOverPlayPtr(QString key="test");
     void ClearAllStdOverPlayPtr();
     //图像视图绘制
     void addOverPlayPtr(void* tpart,QString key="test");
     void removeOverPlayPtr(QString key="test");
     void ClearAllOverPlayPtr();
     void toSetOverPlayOffSet(QString key="test",QPoint OffPos=QPoint(0,0));
     //
     void toViewPoint( double centerx=0, double centery=0,double scale=1.0);//按照缩放倍数把显示点拉到中心
public:
     void toDispimage(YtMutilImDefine &Image); //显示图片

     void toSetBGColor(QColor color);//设置背景色
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
};

#endif // YtSuperShowDisp_H
