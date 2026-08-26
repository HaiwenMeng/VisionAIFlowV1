#ifndef QGView_H
#define QGView_H


#include <QGraphicsView>
#include "YtVisionOverPlay.h"
#include <QScrollBar>
#include <QMenu>

class QLabel;
class QMouseEvent;
class QGScene;
class ImageItem;
class YtRoiShowDisp;
class QGView : public QGraphicsView
{
    Q_OBJECT
public:
    QGView(QWidget *parent = nullptr);
    void ClearObj();
public:
    void toDispimage(QImage &Image); //显示图片
    QRect toGetViewIm();//获取当前图像显示位置
    void toViewPoint( double centerx=0, double centery=0,double scale=1.0);//按照缩放倍数把显示点拉到中心
public:
    QScrollBar *thorizontalScrollBar;
    QScrollBar *tverticalScrollBar;
signals:
    void ScaleChange(qreal scale);
protected:
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void wheelEvent(QWheelEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void drawForeground(QPainter *painter, const QRectF &rect) override;
    virtual void drawBackground(QPainter *painter, const QRectF &rect) override;
    virtual void paintEvent(QPaintEvent *event) override;
public:
    void ZoomFrame(double value);
    void GetFit();
    double m_ZoomValue=1;
    double m_ZoomValueInver=1;
    double m_PixX=0;
    double m_PixY=0;
    QImage m_showimage;//显示图像
    QGScene* m_scene;
    QColor m_BackColor;//背景色

public:
    QLabel *tedit;
public:
    //ROI的list存储
     typedef QMap<QString,  void*> ItemMap;
     ItemMap m_ListItemMap;//存储map
     typedef QMap<QString,  int> ItemMapType;
     ItemMapType m_ListItemMapType;//存储map type
     //视图绘制指针
     ItemMap m_StdOverPlayItemMap;//固定显示对象
     ItemMap m_OverPlayItemMap;//图层显示对象
     //
     YtSetShowtObj m_OverPlayShow;//图层显示
     YtSetShowtObj m_StdOverPlayShow;//固定图层显示
public:
     QString m_IDKey;
     int m_RightType=0;//右键功能
     YtRoiShowDisp *m_YtRoiShowDisp=nullptr;
     QScrollBar *m_QScrollBar=nullptr;
public:
     //绘制图像坐标线
     int m_Setrow=0;
     int m_Setcol=0;
     int m_Setlinewidth=1;
     QColor m_SetLineColor=Qt::blue;
private:
    void toPaintGrayPixVal(QPainter *tpanter);
public:
    void toPaintGridShow(QPainter *tpanter);//绘制图像辅助线
public:
    //ROI 体系
     void addROI(int shape, QVector<double> Val, QString key="Test");//增加ROI
     QVector<double> getROI(QString key="Test");//获取ROI
     void toRemoveRoiByKey(QString key="Test");//斜矩形，圆弧
     void toRemoveAllRoi();//清除所有的ROI
public slots:
     void toSetCurentView(int Value);
     void toSetCurentViewX(int Value);
public:
     QMenu* m_menu;//右键菜单
private slots:
    void slot_triggered(QAction *action);
public:
    void toSaveImage(QString action);
public:
    bool m_IsShowHistor=false;
    void toDrawPrline(QPointF impoint, bool isclear);//绘制线条
public:
    QDateTime m_UpdateTime;
    bool m_Isshowsliter=false;


};

#endif // QGView_H

