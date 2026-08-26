#ifndef YTCHARTVIEW_H
#define YTCHARTVIEW_H

#include <QtCharts>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QLineSeries>
#include <QBarSeries>
#include <QBarSet>


class YtChartView : public QChartView
{
    Q_OBJECT
public:
    explicit YtChartView(QWidget *parent = nullptr);


    //初始化图表
    void toInitChart(QString ChartTitle = u8"");

    //增加折线图
    void toAddLineSerices(QColor color = Qt::red, QString name = u8"测试用例",int axisType = 0);

    //添加或者获取数据
    void toInsertData(int index,int xPos = 0, int y = 0);
    QPoint toGetData(int index = 0, int xPos = 0);

    void toInsertData(int index, int xPos  = 0, float y = 0.0);
    QPointF toGetFloatData(int index, int xPos);

    void toInsertDatas(int index, QVector<int> y);
    QVector<QPoint> toGetIntVecData(int index = 0);

    void toInsertDatas(int index, QVector<float> y);
    QVector<QPointF> toGetFloatVecData(int index = 0);


    //设置统计图颜色
    void toSetColor(int index = 0, QColor color = Qt::red);
    QColor toGetColor(int Index = 0);

    void toSetColors(QVector<int> indexs, QVector<QColor> colors);
    QVector<QColor > toGetColors();


    //设置统计图的宽度
    void toSetWidth(int index = 0, float width = 0.3);
    float toGetWidth(int index = 0);

    void toSetWidths(QVector<float> widths);
    QVector<float> toGetWidths();

    //设置坐标轴的数值
    void toSetXAxis(double min = 0, double max = 10, int Step = 1,int tickCount = 10, QString title =u8"批次", int isRefresh = 0);
    void toSetYAxis(double min = 0, double max = 10, int tickCount = 10, QString title =u8"主坐标轴");
    void toSetOtherAxis(double min = 0.0, double max = 1.0, int tickCount = 10, QString title =u8"副坐标轴");

    //显示十字交叉的线
    void toSetCrosshairVisible(bool visible);

    //移除数据点
    void removePoints(int index);
    void removeAllPoints();

    //设置最值  0:X轴 1:Y轴主轴  2:Y轴副轴
    void toSetMaxVal(int type = 0, int Value = 0);
    void toSetMinVal(int type = 0, int Value = 0);

protected:
    void adjustZoom(const QRectF &selectionRect);

signals:
    void toUpdateAxisVal();

public slots:
    void toSetAxisVal();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;


private:
    QChart                  m_chart;                        //图表
    QVector<QLineSeries *>  m_LineSerices;                  //折线图
    QVector<int>            m_SericesAxisType;              //坐标轴类别 0:Y轴为主坐标轴  1:Y轴副坐标轴

    QPointF                 m_crosshairPos;                 //记录十字标线的位置
    bool                    m_crosshairIsShow = false;      //是否显示

    //十字交叉线显示
    QGraphicsLineItem       *m_XLine = nullptr;
    QGraphicsLineItem       *m_YLine = nullptr;
    QGraphicsLineItem       *m_XShortLine = nullptr;
    QGraphicsLineItem       *m_YShortLine = nullptr;

    //坐标轴的最值
    double                  m_xMin = 0.0, m_xMax = 0.0, m_yMin = 0.0, m_yMax = 0.0, m_yOtherMin = 0.0, m_yOtherMax = 0.0;

    //设置X,Y轴
    QValueAxis              *m_XAxis = nullptr;
    QValueAxis              *m_YAxis = nullptr;
    QValueAxis              *m_YOtherAxis = nullptr;
    int                     m_XTickCount = 0;
    int                     m_YTickCount = 0;
    int                     m_YOtherTickCount = 0;
    QString                 m_xLabel = "";
    int                     m_Step = 1;

    //选择缩放的操作区域
    bool                    m_isSelecting = false;
    QPoint                  m_startPoint;
    QPoint                  m_endPoint;
    QGraphicsRectItem       *m_selectionRect = nullptr;

};

#endif // YTCHARTVIEW_H
