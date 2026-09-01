#include "ytchartview.h"

#include <qDebug>
#include <QValueAxis>
#include <QtMath>
#include <QToolTip>
#include <QBrush>
#include <QPen>

using namespace std;
namespace
{
void applyDarkAxisStyle(QValueAxis *axis)
{
    if (!axis)
    {
        return;
    }

    QPen axisPen(QColor(120, 137, 164));
    axisPen.setWidthF(1.0);

    QPen gridPen(QColor(42, 52, 68));
    gridPen.setWidthF(1.0);

    axis->setLinePen(axisPen);
    axis->setGridLinePen(gridPen);
    axis->setLabelsColor(QColor(210, 222, 240));
    axis->setTitleBrush(QBrush(QColor(230, 237, 247)));
}
} // namespace

void YtChartView::toInitChart(QString ChartTitle)
{
    for (int i = 0; i < m_LineSerices.size(); i++)
    {
        m_chart.addSeries(m_LineSerices.at(i));
    }

    m_XLine = new QGraphicsLineItem();
    m_YLine = new QGraphicsLineItem();
    m_XShortLine = new QGraphicsLineItem();
    m_YShortLine = new QGraphicsLineItem();
    scene()->addItem(m_XLine);
    scene()->addItem(m_YLine);
    scene()->addItem(m_XShortLine);
    scene()->addItem(m_YShortLine);

    m_XLine->setZValue(2);
    m_YLine->setZValue(2);
    m_XShortLine->setZValue(2);
    m_YShortLine->setZValue(2);

    m_chart.setTitle(ChartTitle);
    m_chart.setMargins(QMargins(0, 0, 0, 0));
    m_chart.setBackgroundBrush(QBrush(QColor(14, 20, 30)));
    m_chart.setBackgroundPen(QPen(QColor(42, 52, 68)));
    m_chart.setPlotAreaBackgroundBrush(QBrush(QColor(10, 14, 20)));
    m_chart.setPlotAreaBackgroundVisible(true);
    m_chart.setTitleBrush(QBrush(QColor(230, 237, 247)));

    if (m_chart.legend())
    {
        m_chart.legend()->setLabelColor(QColor(230, 237, 247));
        m_chart.legend()->setBrush(QBrush(QColor(14, 20, 30)));
        m_chart.legend()->setPen(QPen(QColor(42, 52, 68)));
    }

    setBackgroundBrush(QBrush(QColor(14, 20, 30)));
    setStyleSheet(QStringLiteral("background: #0E141E; border: 1px solid #2A3649; border-radius: 8px;"));

    // 显示设置抗锯齿状态
    setRenderHint(QPainter::Antialiasing);
    // 设置橡皮筋选择框(选择缩放框)
    setRubberBand(QChartView::RectangleRubberBand);
    // 设置动画效果
    m_chart.setAnimationOptions(QChart::AllAnimations);
    setChart(&m_chart);

    connect(this, &YtChartView::toUpdateAxisVal, this, &YtChartView::toSetAxisVal);
}

YtChartView::YtChartView(QWidget *parent) : QChartView(parent)
{

    if (m_XLine)
    {
        delete m_XLine;
        m_XLine = nullptr;
    }

    if (m_YLine)
    {
        delete m_YLine;
        m_YLine = nullptr;
    }

    if (m_XShortLine)
    {
        delete m_XShortLine;
        m_XShortLine = nullptr;
    }

    if (m_YShortLine)
    {
        delete m_YShortLine;
        m_YShortLine = nullptr;
    }

    if (m_XAxis)
    {
        delete m_XAxis;
        m_XAxis = nullptr;
    }

    if (m_YAxis)
    {
        delete m_YAxis;
        m_YAxis = nullptr;
    }

    if (m_YOtherAxis)
    {
        delete m_YOtherAxis;
        m_YOtherAxis = nullptr;
    }

    if (m_selectionRect)
    {
        delete m_selectionRect;
        m_selectionRect = nullptr;
    }
}

void YtChartView::toAddLineSerices(QColor color, QString name, int axisType)
{
    QLineSeries *LineSerices = new QLineSeries();

    QPen pen;
    pen.setWidthF(1);
    pen.setColor(color);
    LineSerices->setPen(pen);

    LineSerices->setName(name);
    m_LineSerices.append(LineSerices);
    m_SericesAxisType.append(axisType);
}

void YtChartView::toInsertData(int index, int xPos, int y)
{
    if (index < 0 || index >= m_LineSerices.size())
    {
        return;
    }

    // 如果是坐标轴
    if (m_SericesAxisType.at(index) == 0)
    {
        if (y >= m_yMax)
        {
            m_yMax = y;
            emit toUpdateAxisVal();
        }
        else if (y <= m_yMin)
        {
            m_yMin = y;
            emit toUpdateAxisVal();
        }
    }
    else
    {
        if (y >= m_yOtherMax)
        {
            m_yOtherMax = y;
            emit toUpdateAxisVal();
        }
        else if (y <= m_yOtherMin)
        {
            m_yOtherMin = y;
            emit toUpdateAxisVal();
        }
    }

    m_LineSerices.at(index)->append(xPos, y);
}

QPoint YtChartView::toGetData(int index, int xPos)
{
    QPoint point = QPoint(0, 0);
    if (index < 0 || index > m_LineSerices.size())
    {
        return point;
    }

    point = QPoint(m_LineSerices.at(index)->at(xPos).x(), m_LineSerices.at(index)->at(xPos).y());

    return point;
}

void YtChartView::toInsertData(int index, int xPos, float y)
{
    if (index < 0 || index >= m_LineSerices.size())
    {
        return;
    }
    // 如果是坐标轴
    if (m_SericesAxisType.at(index) == 0)
    {
        if (y >= m_yMax)
        {
            m_yMax = y;
            emit toUpdateAxisVal();
        }
        else if (y <= m_yMin)
        {
            m_yMin = y;
            emit toUpdateAxisVal();
        }
    }
    else
    {
        if (y >= m_yOtherMax)
        {
            m_yOtherMax = y;
            emit toUpdateAxisVal();
        }
        else if (y <= m_yOtherMin)
        {
            m_yOtherMin = y;
            emit toUpdateAxisVal();
        }
    }

    m_LineSerices.at(index)->append(xPos, y);
    emit toUpdateAxisVal();
}

QPointF YtChartView::toGetFloatData(int index, int xPos)
{
    QPointF ret = QPointF(0.0, 0.0);
    if (index < 0 || index > m_LineSerices.size())
    {
        return ret;
    }

    ret = m_LineSerices.at(index)->at(xPos);
    return ret;
}

void YtChartView::toInsertDatas(int index, QVector<int> y)
{
    if (index < 0 || index > m_LineSerices.size())
    {
        return;
    }

    int length = (y.size() < m_LineSerices.at(index)->count()) ? y.size() : m_LineSerices.at(index)->count();

    for (int i = 0; i < length; i++)
    {
        // 如果是坐标轴
        if (m_SericesAxisType.at(index) == 0)
        {
            if (y.at(i) >= m_yMax)
            {
                m_yMax = y.at(i);
                emit toUpdateAxisVal();
            }
            else if (y.at(i) <= m_yMin)
            {
                m_yMin = y.at(i);
                emit toUpdateAxisVal();
            }
        }
        else
        {
            if (y.at(i) >= m_yOtherMax)
            {
                m_yOtherMax = y.at(i);
                emit toUpdateAxisVal();
            }
            else if (y.at(i) <= m_yOtherMin)
            {
                m_yOtherMin = y.at(i);
                emit toUpdateAxisVal();
            }
        }
        m_LineSerices.at(index)->replace(i, m_LineSerices.at(index)->at(i).x(), y.at(i));
    }
}

QVector<QPoint> YtChartView::toGetIntVecData(int index)
{
    QVector<QPoint> datas;
    if (index < 0 || index > m_LineSerices.size())
    {
        return datas;
    }

    for (int i = 0; i < m_LineSerices.at(index)->count(); i++)
    {
        QPoint point = QPoint(m_LineSerices.at(index)->at(i).x(), m_LineSerices.at(index)->at(i).y());
        datas.append(point);
    }

    return datas;
}

void YtChartView::toInsertDatas(int index, QVector<float> y)
{
    if (index < 0 || index > m_LineSerices.size())
    {
        return;
    }

    int length = (y.size() < m_LineSerices.at(index)->count()) ? y.size() : m_LineSerices.at(index)->count();

    for (int i = 0; i < length; i++)
    {
        // 如果是坐标轴
        if (m_SericesAxisType.at(index) == 0)
        {
            if (y.at(i) >= m_yMax)
            {
                m_yMax = y.at(i);
                emit toUpdateAxisVal();
            }
            else if (y.at(i) <= m_yMin)
            {
                m_yMin = y.at(i);
                emit toUpdateAxisVal();
            }
        }
        else
        {
            if (y.at(i) >= m_yOtherMax)
            {
                m_yOtherMax = y.at(i);
                emit toUpdateAxisVal();
            }
            else if (y.at(i) <= m_yOtherMin)
            {
                m_yOtherMin = y.at(i);
                emit toUpdateAxisVal();
            }
        }
        m_LineSerices.at(index)->replace(i, m_LineSerices.at(index)->at(i).x(), y.at(i));
    }
}

QVector<QPointF> YtChartView::toGetFloatVecData(int index)
{
    QVector<QPointF> datas;
    if (index < 0 || index > m_LineSerices.size())
    {
        return datas;
    }

    for (int i = 0; i < m_LineSerices.at(index)->count(); i++)
    {
        QPointF point = m_LineSerices.at(index)->at(i);
        datas.append(point);
    }

    return datas;
}

void YtChartView::toSetColor(int index, QColor color)
{
    if (index < 0 || index > m_LineSerices.size())
    {
        return;
    }

    m_LineSerices.at(index)->setColor(color);
}

QColor YtChartView::toGetColor(int Index)
{
    QColor color = Qt::red;

    if (Index > 0 && Index < m_LineSerices.size())
    {
        color = m_LineSerices.at(Index)->color();
    }

    return color;
}

void YtChartView::toSetColors(QVector<int> indexs, QVector<QColor> colors)
{
    if (m_LineSerices.size() <= 0)
    {
        return;
    }

    for (int i = 0; i < indexs.size(); i++)
    {
        m_LineSerices.at(i)->setColor(colors.at(i));
    }
}

QVector<QColor> YtChartView::toGetColors()
{
    QVector<QColor> colors;

    colors.clear();
    for (int i = 0; i < colors.size(); i++)
    {
        colors.append(m_LineSerices.at(i)->color());
    }

    return colors;
}

void YtChartView::toSetWidth(int index, float width)
{
    if (index < 0 || index > m_LineSerices.size())
    {
        return;
    }

    QPen pen = m_LineSerices.at(index)->pen();
    pen.setWidthF(width);
    m_LineSerices.at(index)->setPen(pen);
}

float YtChartView::toGetWidth(int index)
{

    float width = 0.0;
    if (index < 0 || index > m_LineSerices.size())
    {
        return width;
    }

    QPen pen = m_LineSerices.at(index)->pen();
    width = pen.widthF();

    return width;
}

void YtChartView::toSetWidths(QVector<float> widths)
{
    int len = (widths.size() < m_LineSerices.size()) ? widths.size() : m_LineSerices.size();

    for (int i = 0; i < len; i++)
    {
        QPen pen = m_LineSerices.at(i)->pen();
        pen.setWidthF(widths.at(i));
        m_LineSerices.at(i)->setPen(pen);
    }
}

QVector<float> YtChartView::toGetWidths()
{
    QVector<float> widths;
    widths.clear();
    for (int i = 0; i < m_LineSerices.size(); i++)
    {
        QPen pen = m_LineSerices.at(i)->pen();
        widths.append(pen.widthF());
    }

    return widths;
}

void YtChartView::toSetXAxis(double min, double max, int Step, int tickCount, QString title, int isRefresh)
{
    if (min > max)
    {
        return;
    }

    m_xMin = min;
    m_xMax = max;
    m_XTickCount = tickCount;
    m_xLabel = title;
    m_Step = Step;

    if (isRefresh)
    {
        emit toUpdateAxisVal();
        return;
    }

    if (m_XAxis == nullptr)
    {
        m_XAxis = new QValueAxis();
    }

    m_XAxis->setRange(min, max);
    m_XAxis->setTickCount(tickCount);
    m_XAxis->setTitleText(title);
    m_XAxis->setLabelFormat("%d");
    applyDarkAxisStyle(m_XAxis);
    m_chart.addAxis(m_XAxis, Qt::AlignBottom);

    for (int i = 0; i < m_SericesAxisType.size(); i++)
    {
        m_chart.setAxisX(m_XAxis, m_LineSerices.at(i));
    }
}

void YtChartView::toSetYAxis(double min, double max, int tickCount, QString title)
{
    if (min > max)
    {
        return;
    }

    m_yMin = min;
    m_yMax = max;
    m_YTickCount = tickCount;

    if (m_YAxis == nullptr)
    {
        m_YAxis = new QValueAxis();
    }

    m_YAxis->setRange(min, max);
    m_YAxis->setTickCount(tickCount);
    m_YAxis->setTitleText(title);
    applyDarkAxisStyle(m_YAxis);
    m_chart.addAxis(m_YAxis, Qt::AlignLeft);

    for (int i = 0; i < m_SericesAxisType.size(); i++)
    {
        if (m_SericesAxisType.at(i) == 0)
        {
            m_chart.setAxisY(m_YAxis, m_LineSerices.at(i));
        }
    }
}

void YtChartView::toSetOtherAxis(double min, double max, int tickCount, QString title)
{
    if (min > max)
    {
        return;
    }

    m_yOtherMin = min;
    m_yOtherMax = max;
    m_YOtherTickCount = tickCount;

    if (m_YOtherAxis == nullptr)
    {
        m_YOtherAxis = new QValueAxis();
    }
    m_YOtherAxis->setRange(min, max);
    m_YOtherAxis->setTickCount(tickCount);
    m_YOtherAxis->setTitleText(title);
    applyDarkAxisStyle(m_YOtherAxis);
    m_chart.addAxis(m_YOtherAxis, Qt::AlignRight);

    for (int i = 0; i < m_SericesAxisType.size(); i++)
    {
        if (m_SericesAxisType.at(i) == 1)
        {
            m_chart.setAxisY(m_YOtherAxis, m_LineSerices.at(i));
        }
    }
}

void YtChartView::toSetCrosshairVisible(bool visible)
{
    m_crosshairIsShow = visible;
    viewport()->update();
}

void YtChartView::removePoints(int index)
{
    if (index < 0 || index > m_LineSerices.size())
    {
        return;
    }

    if (m_LineSerices.at(index)->points().isEmpty())
    {
        return;
    }

    for (int i = 0; i < m_LineSerices.at(index)->count(); i++)
    {
        m_LineSerices.at(index)->replace(i, m_LineSerices.at(index)->at(i).x(), 0);
    }
}

void YtChartView::removeAllPoints()
{
    for (int i = 0; i < m_LineSerices.size(); i++)
    {
        m_LineSerices.at(i)->clear();
    }

    toSetXAxis(0, m_xMax, m_Step, m_XTickCount, m_xLabel, 1);
}

void YtChartView::toSetMaxVal(int type, int Value)
{
    if (type == 0)
    {
        m_xMax = Value;
    }
    else if (type == 1)
    {
        m_yMax = Value;
    }
    else if (type == 2)
    {
        m_yOtherMax = Value;
    }
}

void YtChartView::toSetMinVal(int type, int Value)
{
    if (type == 0)
    {
        m_xMin = Value;
    }
    else if (type == 1)
    {
        m_yMin = Value;
    }
    else if (type == 2)
    {
        m_yOtherMin = Value;
    }
}

void YtChartView::adjustZoom(const QRectF &selectionRect)
{
    // 获取矩形区域的坐标
    QPointF Pos = chart()->mapToValue(QPointF(selectionRect.x(), selectionRect.y()));
    QPointF Poss = chart()->mapToValue(
        QPointF(selectionRect.x() + selectionRect.width(), selectionRect.y() + selectionRect.height()));
    ;

    // 获取当前缩放区域Y向最值
    double XMin = (Pos.x() < Poss.x()) ? Pos.x() : Poss.x();
    double XMax = (Pos.x() < Poss.x()) ? Poss.x() : Pos.x();
    double YMin = (Pos.y() < Poss.y()) ? Pos.y() : Poss.y();
    double YMax = (Pos.y() < Poss.y()) ? Poss.y() : Pos.y();

    double OtherXMin = 0, OtherXMax = 0;
    if (m_YOtherAxis != nullptr)
    {
        // 根据Y主坐标轴的比例缩放副坐标轴
        double scaleMin = YMin / m_yMax;
        double scaleMax = YMax / m_yMax;

        OtherXMin = m_yOtherMax * scaleMin;
        OtherXMax = m_yOtherMax * scaleMax;
    }

    // 如果选择的区域有效,则调整坐标轴的范围
    if (XMin < XMax && YMin < YMax)
    {
        emit toUpdateAxisVal();
    }
}

void YtChartView::toSetAxisVal()
{
    m_XAxis->setRange(m_xMin, m_xMax);
    m_YAxis->setRange(m_yMin, m_yMax);
    if (m_YOtherAxis != nullptr)
    {
        m_YOtherAxis->setRange(m_yOtherMin, m_yOtherMax);
    }
}

void YtChartView::wheelEvent(QWheelEvent *event)
{

    // 获取鼠标当前场景的坐标
    QPointF pos = chart()->mapToValue(event->position());

    // 设置缩放比例
    qreal retVal = 0.0;
    // 根据滚轮的方向调整坐标轴范围
    const int wheelDelta = event->angleDelta().y();
    if (wheelDelta > 0)
    {
        // 向上滚动，缩小显示区域
        retVal = std::pow(0.999, wheelDelta);

        // 读取视图基本信息
        QRectF plotAreaRect = chart()->plotArea();
        QPointF centerPoint = plotAreaRect.center();

        // 水平调整
        plotAreaRect.setWidth(plotAreaRect.width() * retVal);
        plotAreaRect.setHeight(plotAreaRect.height() * retVal);

        QPointF newCenterPoint(2 * centerPoint - event->position() - (centerPoint - event->position()) / retVal);

        plotAreaRect.moveCenter(newCenterPoint);
        chart()->zoomIn(plotAreaRect);
        QChartView::wheelEvent(event);
    }
    else
    {
        chart()->zoom(0.95);
    }
}

void YtChartView::mousePressEvent(QMouseEvent *event)
{
    // 鼠标左键点击做出响应
    if (event->button() == Qt::LeftButton)
    {
        // 获取鼠标移动的视图坐标
        QPointF scenPos = chart()->mapToValue(event->pos());

        if ((scenPos.x() >= m_xMin && scenPos.x() <= m_xMax) && (scenPos.y() >= m_yMin && scenPos.y() <= m_yMax))
        {
            m_startPoint = event->pos(); // 记录起始点
            m_isSelecting = true;
            m_endPoint = m_startPoint;

            // 如果选择的矩形存在,则删除
            if (m_selectionRect)
            {
                scene()->removeItem(m_selectionRect);
                delete m_selectionRect;
            }

            // 创建新的矩形
            m_selectionRect = new QGraphicsRectItem();
            m_selectionRect->setBrush(QBrush(QColor(0, 0, 255, 100))); // 设置半透明蓝色
            m_selectionRect->setPen(QPen(QColor(0, 0, 255)));          // 设置透明边框
            scene()->addItem(m_selectionRect);
        }
    }
    else if (event->button() == Qt::RightButton)
    {
        emit toUpdateAxisVal();
        chart()->zoomReset();
    }
}

void YtChartView::mouseMoveEvent(QMouseEvent *event)
{
    // 获取鼠标移动的视图坐标
    QPointF scenPos = chart()->mapToValue(event->pos());

    QChartView::mouseMoveEvent(event);

    /*---------------------------------------十字交叉线显示---------------------------------------------*/
    // 设置十字交叉线的显示
    QValueAxis *x = (QValueAxis *)chart()->axisX();
    QValueAxis *y = (QValueAxis *)chart()->axisY();

    //    if(m_crosshairIsShow  && (scenPos.x() >= m_xMin && scenPos.x() <= m_xMax) && (scenPos.y() >= m_yMin &&
    //    scenPos.y() <= m_yMax))
    if (m_crosshairIsShow && (scenPos.x() >= x->min() && scenPos.x() <= x->max()) &&
        (scenPos.y() >= y->min() && scenPos.y() <= y->max()))
    {
        m_XLine->setVisible(true);
        m_XShortLine->setVisible(true);

        m_YLine->setVisible(true);
        m_YShortLine->setVisible(true);

        QPen pen;
        pen.setColor(Qt::darkCyan);
        pen.setWidth(0.5);
        pen.setStyle(Qt::DashLine);
        m_XLine->setPen(pen);
        m_YLine->setPen(pen);
        m_XLine->setLine(event->pos().x(), 0, event->pos().x(), this->height());
        m_YLine->setLine(0, event->pos().y(), this->width(), event->pos().y());

        pen.setStyle(Qt::SolidLine);
        pen.setWidth(5);
        m_XShortLine->setLine(event->pos().x(), event->pos().y() - 50, event->pos().x(), event->pos().y() + 50);
        m_YShortLine->setLine(event->pos().x() - 50, event->pos().y(), event->pos().x() + 50, event->pos().y());
    }
    else
    {
        m_XLine->setVisible(false);
        m_XShortLine->setVisible(false);

        m_YLine->setVisible(false);
        m_YShortLine->setVisible(false);
    }

    /*---------------------------------------缩放矩形框显示---------------------------------------------*/
    // 显示选中的矩形框
    if (m_isSelecting)
    {
        m_endPoint = event->pos(); // 更新结束点
        // 获取选中的矩形区域
        QRect rect(m_startPoint, m_endPoint);
        // 更新矩形区域，nomalized保证矩形区域的坐标顺序是正确的
        m_selectionRect->setRect(rect.normalized());
    }

    /*---------------------------------------显示鼠标位置附近的点数据---------------------------------------------*/
    // 靠近10个像素再显示
    const int PosThreshold = 10;
    bool isFound = false;

    for (int i = 0; i < m_LineSerices.size(); i++)
    {
        for (int j = 0; j < m_LineSerices.at(i)->count(); j++)
        {
            QPointF point = m_LineSerices.at(i)->at(j);
            QPointF viewPoint = chart()->mapToPosition(point);

            // 检查是否在点附近
            if (m_SericesAxisType.at(i) == 0)
            {
                if (qAbs(viewPoint.x() - event->pos().x()) < PosThreshold &&
                    qAbs(viewPoint.y() - event->pos().y()) < PosThreshold)
                {
                    QToolTip::showText(
                        event->globalPos(),
                        QString("%1\nX:%2\nY:%3").arg(m_LineSerices.at(i)->name()).arg(point.x()).arg(point.y()));
                    isFound = true;
                    break;
                }
            }
            else
            {
                if (qAbs(viewPoint.x() - event->pos().x()) < PosThreshold)
                {
                    QToolTip::showText(
                        event->globalPos(),
                        QString("%1\nX:%2\nY:%3").arg(m_LineSerices.at(i)->name()).arg(point.x()).arg(point.y()));
                    isFound = true;
                    break;
                }
            }

            if (isFound)
            {
                break;
            }
        }
    }

    if (!isFound)
    {
        QToolTip::hideText();
    }
}

void YtChartView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (m_isSelecting)
        {
            m_isSelecting = false;
            // 获取矩形区域的坐标
            QRectF selectRect = m_selectionRect->rect();

            // 根据选择的区域调整坐标轴的范围
            adjustZoom(selectRect);

            // 删除选中的区域
            scene()->removeItem(m_selectionRect);
            delete m_selectionRect;
            m_selectionRect = nullptr;
        }
    }
}
