#ifndef YtMapShowDisp_H
#define YtMapShowDisp_H
#include <QWidget>
#include <QtUiPlugin/QDesignerExportWidget>
struct YtSetShowtObj;
namespace Ui {
class YtMapShowDisp;
}

class QDESIGNER_WIDGET_EXPORT YtMapShowDisp : public QWidget
{
    Q_OBJECT

public:
    explicit YtMapShowDisp(QWidget *parent = nullptr);
    ~YtMapShowDisp();

private:
    Ui::YtMapShowDisp *ui;

public:
    void toSetBGColor(QColor color);//设置背景色
    void toUpdateShow(bool isfitshow=false);//刷新显示
    void toSetMapSize(QSizeF SetSize);//设置画布
    void toSetAxisOPos(QPointF SetCenter,QColor Showcolr=Qt::white);//设置原点位置
    void toSetLeftDoubleMode(int t=0,QString key="1");//0 为正常模式，

public:
    //开始管理图层
    void toAddItem(QString Key,QRectF SetPos);
    void toRemoveItem(QString Key);
    void toRemovAllItem();
    //管理key的图层
    YtSetShowtObj * toGetItemOverPlay(QString Key);
    void toSetShowBgIm(QString Key,QImage &Setim);
public:
    void toSetItemPos(QString Key,QRectF SetPos);
    void toSetItemOpacity(QString Key,double Setval=0.5);
    QRectF toGetItemPos(QString Key);
    void toSetShowTip(QString Key,bool isshow);
    void toSetAllShowTip(bool isshow);
    void toGetInnerPixIm(QVector<QPoint> &impos,QVector<QImage> &getim, QRect &OutSize);//借助内部去计算multi的保存

public:
    void toViewPoint( double centerx=0, double centery=0,double scale=1.0);//按照缩放倍数把显示点拉到中心
    void toSetSliderView(bool istrue);//设置侧边查看
    QImage toGetSizeIm(QSize DefineSize);//尝试把场景设置指定大小，计算截图
signals:
    void toGetPosInfo(QPointF Pos);//获取平面坐标
    void toGetPhiPos(double R,double Ang);
    void toLeftDoubleOut(QString key);
    void toSigGetCurentPosScal(QPoint Pos,double scale);
public:
    void toSlotSetCurentPosScal(QPoint Pos,double scale);



};
void Q_DECL_EXPORT toSSMoveOveplay(YtSetShowtObj &Main, YtSetShowtObj &temres, double MoveX, double MoveY, double scalx=1, double scaly=1);

#endif // YtMapShowDisp_H
