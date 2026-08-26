#include "QGView.h"
#include <QScrollBar>
#include <QDebug>
#include <QFileDialog>
#include <QAction>
#include <QLabel>
#include "QGScene.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QWheelEvent>
#include "ytvisiondefine.h"
#include "ytroishowdisp.h"
#include "ROIItem/QtCircleROI.h"
#include "ROIItem/QtRectROI.h"
#include "ROIItem/QtPointROI.h"
#include "ROIItem/QtEllipseROI.h"
#include "ROIItem/QtRotateRectROI.h"
#include "ROIItem/QtRingROI.h"
#include "ROIItem/QtArcROI.h"
#include "ROIItem/QtPieROI.h"
#include "ROIItem/QtPolygonROI.h"
#include "ROIItem/QtPolygonROIE.h"
#include "ROIItem/QtLineROI.h"
#include "ROIItem/QtLineROISeg.h"
#include "ROIItem/QtWaistShapeROI.h"
QGView::QGView(QWidget *parent) : QGraphicsView(parent)
{
    this->setViewportUpdateMode(QGraphicsView::FullViewportUpdate); //解决拖动是背景图片残影
    setDragMode(QGraphicsView::ScrollHandDrag);
    //反锯齿
    setRenderHints(QPainter::Antialiasing);
    // 隐藏水平/竖直滚动条
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    /*以鼠标中心进行缩放*/
    setMouseTracking(true);
    setTransformationAnchor(
        QGraphicsView::AnchorUnderMouse); //设置视口变换的锚点，这个属性控制当视图做变换时应该如何摆放场景的位置
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    // 设置场景范围
    setSceneRect(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
    m_scene = new QGScene(this);

    this->setScene(m_scene);
    m_BackColor = QColor(128, 128, 128);
    tedit = new QLabel(this);
    tedit->setStyleSheet(QString("background-color: black;color:white;font:bold 8pt \"微软雅黑\";"));
    m_OverPlayItemMap.insert("INERRSHOW", &m_OverPlayShow);
    m_StdOverPlayItemMap.insert("INERRSHOW", &m_StdOverPlayShow);
    ///////////////////////
    m_menu = new QMenu;
    m_menu->addAction(new QAction(QIcon(), u8"全屏/常规显示", this));
    m_menu->addAction(new QAction(QIcon(), u8"适应图像显示", this));
    m_menu->addAction(new QAction(QIcon(), u8"显示/隐藏行列直方信息", this));
    m_menu->addAction(new QAction(QIcon(), u8"显示/隐藏侧边栏", this));

    m_menu->addSeparator();
    m_menu->addAction(new QAction(QIcon(), u8"保存原图...", this));
    m_menu->addAction(new QAction(QIcon(), u8"保存截图...", this));
    m_menu->addAction(new QAction(QIcon(), u8"保存渲染图..", this));
    connect(m_menu, SIGNAL(triggered(QAction *)), this, SLOT(slot_triggered(QAction *)));
    m_UpdateTime = QDateTime::currentDateTime();
}

void QGView::toDispimage(QImage &Image)
{
    if (m_showimage.size() == Image.size())
    {
        m_showimage = Image;
        m_scene->update();
    }
    else
    {
        m_showimage = Image;
        GetFit();
    }
}

QRect QGView::toGetViewIm()
{
    QScrollBar *pHbar = this->horizontalScrollBar();
    QScrollBar *pVbar = this->verticalScrollBar();
    qDebug() << this->rect();
    qDebug() << pHbar->minimum() << pHbar->maximum() << pHbar->value() << pHbar->pos();
    qDebug() << pVbar->minimum() << pVbar->maximum() << pVbar->value() << pVbar->pos();

    QRect rect;
    if (m_showimage.isNull())
    {
        return rect;
    }
    QPointF tTopLeft = mapToScene(this->rect().topLeft());
    QPointF tBottomRight = mapToScene(this->rect().bottomRight());
    rect = QRect(
        QPoint(YtMAX(0, tTopLeft.x()), YtMAX(0, tTopLeft.y())),
        QPoint(YtMIN(tBottomRight.x() + 1, m_showimage.width()), YtMIN(tBottomRight.y() + 1, m_showimage.height())));
    return rect;
}

void QGView::toViewPoint(double centerx, double centery, double scale)
{
    //图片自适应方法
    qreal winWidth = this->width();
    qreal winHeight = this->height();
    m_ZoomValue = scale;
    m_ZoomValueInver = 1.0 / m_ZoomValue;
    //主要就是计算以图像原点的显示点到显示框的左上点的偏移
    m_PixX = centerx * m_ZoomValue - winWidth / 2;
    m_PixY = centery * m_ZoomValue - winHeight / 2;
    ;
    ZoomFrame(m_ZoomValue);
    QScrollBar *pHbar = this->horizontalScrollBar();

    pHbar->setSliderPosition(m_PixX);
    QScrollBar *pVbar = this->verticalScrollBar();
    pVbar->setSliderPosition(m_PixY);
}

void QGView::ClearObj()
{
    foreach (auto item, m_scene->items())
    {
        m_scene->removeItem(item);
    }
}

void QGView::ZoomFrame(double value)
{
    m_scene->UpDateZoom(1.0 / value);
    this->setTransform(QTransform(value, 0, 0, value, 0, 0));
}

void QGView::GetFit()
{
    if (this->width() < 1 || m_showimage.width() < 1)
    {
        return;
    }
    //图片自适应方法
    qreal winWidth = this->width();
    qreal winHeight = this->height();
    qreal ScaleWidth = (m_showimage.width() + 1) / winWidth;
    qreal ScaleHeight = (m_showimage.height() + 1) / winHeight;
    qreal row1, column1;
    if (ScaleWidth >= ScaleHeight)
    {
        row1 = -(1) * ((winHeight * ScaleWidth) - m_showimage.height()) / 2;
        column1 = 0;
        m_ZoomValue = 1 / ScaleWidth;
    }
    else
    {
        row1 = 0;
        column1 = -(1.0) * ((winWidth * ScaleHeight) - m_showimage.width()) / 2;
        m_ZoomValue = 1 / ScaleHeight;
    }
    m_ZoomValueInver = 1.0 / m_ZoomValue;
    m_PixX = column1 * m_ZoomValue;
    m_PixY = row1 * m_ZoomValue;
    ZoomFrame(m_ZoomValue);
    QScrollBar *pHbar = this->horizontalScrollBar();
    pHbar->setSliderPosition(m_PixX);
    QScrollBar *pVbar = this->verticalScrollBar();
    pVbar->setSliderPosition(m_PixY);
}

void QGView::toPaintGrayPixVal(QPainter *tpanter)
{
    // m_OverPlayShow.m_DispPointS.clear();
    if (m_ZoomValue < 50)
    {
        return;
    }
    QRectF rect;
    QPointF tTopLeft = mapToScene(this->rect().topLeft());
    QPointF tBottomRight = mapToScene(this->rect().bottomRight());
    rect = QRectF(
        QPoint(YtMAX(0, tTopLeft.x()), YtMAX(0, tTopLeft.y())),
        QPoint(YtMIN(tBottomRight.x() + 1, m_showimage.width()), YtMIN(tBottomRight.y() + 1, m_showimage.height())));

    QRgb pixValue;
    QString SetStr;
    QVector<QPointF> pointsOnScreen;
    QVector<QString> valueString;
    for (int y = rect.y(); y < rect.y() + rect.height(); y++)
    {
        for (int x = rect.x(); x < rect.x() + rect.width(); x++)
        {
            pixValue = m_showimage.pixel(x, y);
            if (m_showimage.format() == QImage::Format_Indexed8 || m_showimage.format() == QImage::Format_Grayscale8)
            {

                SetStr = QString::number(qGray(pixValue));
            }
            else if (m_showimage.format() == QImage::Format_Grayscale16)
            {
                SetStr = QString::number(*((ushort *)(m_showimage.bits() + (y * m_showimage.width() + x) * 2)));
            }
            else
            {
                SetStr = QString("%1|%2|%3").arg(qRed(pixValue)).arg(qGreen(pixValue)).arg(qBlue(pixValue));
            }
            pointsOnScreen.append(mapFromScene(QPointF(x + 0.5, y + 0.5)));
            valueString.append(SetStr);
        }
    }
    QPen pen(Qt::gray);
    tpanter->setPen(pen);
    QFont font;
    font.setFamily("Times");
    font.setPointSize(8);
    tpanter->setFont(font);
    for (int i = 0; i < pointsOnScreen.size(); ++i)
    {
        int strWidth = tpanter->fontMetrics().horizontalAdvance(valueString[i]);
        int strHeight = tpanter->fontMetrics().height();
        QPointF fontPoint = QPointF(pointsOnScreen[i] - QPointF(strWidth / 2.0, -strHeight / 4.0));
        QRect fontRect(fontPoint.x() - 1,
                       fontPoint.y() - strHeight + strHeight / 8,
                       strWidth + 2,
                       strHeight + strHeight / 8);
        tpanter->setPen(Qt::gray);
        tpanter->setBrush(QBrush(Qt::gray));
        tpanter->drawRect(fontRect);
        tpanter->setPen(Qt::black);
        tpanter->drawText(fontPoint, valueString[i]);
    }
}

void QGView::toPaintGridShow(QPainter *tpanter)
{
    if (m_Setrow < 1 || m_Setcol < 1)
    {
        return;
    }
    QPen tpen;
    tpen.setColor(m_SetLineColor);
    tpen.setWidthF(qAbs(m_Setlinewidth) / m_ZoomValue);
    if (0 > m_Setlinewidth)
    {
        tpen.setStyle(Qt::DashLine);
    }
    else
    {
        tpen.setStyle(Qt::SolidLine);
    }
    tpanter->setPen(tpen);
    QVector<QLineF> Setlins;
    for (int i = 0; i < m_Setrow; i++)
    {
        Setlins.append(QLineF(0,
                              1.0 * (i + 1) * m_showimage.height() / (m_Setrow + 1),
                              m_showimage.width(),
                              1.0 * (i + 1) * m_showimage.height() / (m_Setrow + 1)));
    }
    for (int i = 0; i < m_Setcol; i++)
    {
        Setlins.append(QLineF(1.0 * (i + 1) * m_showimage.width() / (m_Setcol + 1),
                              0,
                              1.0 * (i + 1) * m_showimage.width() / (m_Setcol + 1),
                              m_showimage.height()));
    }
    for (int i = 0; i < Setlins.size(); i++)
    {
        tpanter->drawLine(Setlins[i]);
    }
}

void QGView::addROI(int shape, QVector<double> Val, QString key)
{
    BaseItem *SetPItem = nullptr;
    toRemoveRoiByKey(key);

    switch (shape)
    {
    case circleROI:
    {
        SetPItem = new QtCircleROI(Val, key);
        break;
    }
    case lineSegCabROI:
    {
        SetPItem = new QtLineROI(Val, key);
        break;
    }
    case rectangleROI:
    {
        SetPItem = new QtRectROI(Val, key);
        break;
    }
    case pointROI:
    {
        SetPItem = new QtPointROI(Val, key);
        break;
    }
    case ellipseROI:
    {
        SetPItem = new QtEllipseROI(Val, key);
        break;
    }
    case rotaterectangleROI:
    {
        SetPItem = new QtRotateRectROI(Val, key);
        break;
    }
    case ringROI:
    {
        SetPItem = new QtRingROI(Val, key);
        break;
    }
    case polygonROI:
    {
        SetPItem = new QtPolygonROI(Val, key);
        break;
    }
    case polygonROIE:
    {
        SetPItem = new QtPolygonROIE(Val, key);
        break;
    }
    case pieROI:
    {
        SetPItem = new QtPieROI(Val, key);
        break;
    }
    case arcROI:
    {
        SetPItem = new QtArcROI(Val, key);
        break;
    }
    case lineSegROI:
    {
        SetPItem = new QtLineROISeg(Val, key);
        break;
    }
    case waistROI:
    {
        SetPItem = new QtWaistShapeROI(Val, key);
        break;
    }
    default:
        break;
    }

    if (SetPItem)
    {
        m_ListItemMap.insert(key, (void *)SetPItem);
        m_ListItemMapType.insert(key, shape);
        m_scene->addItem(SetPItem);
        SetPItem->toSetParent(m_YtRoiShowDisp);
    }
    m_scene->UpDateZoom(1.0 / m_ZoomValue);
}

QVector<double> QGView::getROI(QString key)
{

    if (m_ListItemMap.contains(key)) //判断map里是否已经包含某“键-值”
    {
        BaseItem *temite = (BaseItem *)m_ListItemMap.value(key);
        return temite->toGetDatavalue();
    }
    else
    {
        return QVector<double>();
    }
}

void QGView::toRemoveRoiByKey(QString key)
{
    if (m_ListItemMap.contains(key)) //判断map里是否已经包含某“键-值”
    {
        BaseItem *temite = (BaseItem *)m_ListItemMap.value(key);
        // m_scene->removeItem(temite);
        m_ListItemMap.remove(key);
        delete temite;
    }
}

void QGView::toRemoveAllRoi()
{
    foreach (QString key, m_ListItemMap.keys())
    {
        if (m_ListItemMap.contains(key)) //判断map里是否已经包含某“键-值”
        {
            BaseItem *temite = (BaseItem *)m_ListItemMap.value(key);
            // m_scene->removeItem(temite);
            m_ListItemMap.remove(key);
            delete temite;
        }
    }
}

void QGView::toSetCurentView(int Value)
{
    //主要就是计算以图像原点的显示点到显示框的左上点的偏移
    double stey = Value * m_ZoomValue;
    QScrollBar *pVbar = this->verticalScrollBar();
    pVbar->setSliderPosition(stey);
}

void QGView::toSetCurentViewX(int Value)
{
    double stey = Value * m_ZoomValue;
    QScrollBar *pVbar = this->horizontalScrollBar();
    pVbar->setSliderPosition(stey);
}

void QGView::slot_triggered(QAction *action)
{
    QString actionText = action->text();
    if (actionText == u8"全屏/常规显示")
    {
        if (this->isFullScreen())
        {
            this->setWindowFlags(Qt::SubWindow);
            this->showNormal();
        }
        else
        {
            this->setWindowFlags(Qt::Window);
            this->showFullScreen();
        }
    }
    else if (actionText == u8"适应图像显示")
    {
        GetFit();
    }
    else if (actionText == u8"显示/隐藏行列直方信息")
    {
        m_IsShowHistor = !m_IsShowHistor;
        m_scene->update();
    }
    else if (actionText == u8"显示/隐藏侧边栏")
    {
        m_Isshowsliter = !m_Isshowsliter;
        m_YtRoiShowDisp->toSetSliderView(m_Isshowsliter);
    }
    else
    {
        toSaveImage(actionText);
    }
}

void QGView::toSaveImage(QString action)
{
    QString filename = QFileDialog::getSaveFileName(this, action, "", tr("*.jpg;;*.png;;*.bmp"));
    if (filename.isEmpty())
    {
        return;
    }
    if (action.contains(u8"原图"))
    {
        m_showimage.save(filename);
    }
    if (action.contains(u8"截图"))
    {
        QImage tim = this->grab().toImage();
        tim.save(filename);
    }
    if (action.contains(u8"渲染图"))
    {

        m_YtRoiShowDisp->toGetImPaint().save(filename);
    }
}

void QGView::toDrawPrline(QPointF impoint, bool isclear)
{
    m_OverPlayShow.toClearData();
    if ((!isclear) || (!m_IsShowHistor) || (m_ZoomValue > 50))
    {
        m_scene->update();
        return;
    }
    /////////////////////////////////////////
    QRectF rect;
    QPointF tTopLeft = mapToScene(this->rect().topLeft());
    QPointF tBottomRight = mapToScene(this->rect().bottomRight());
    rect = QRectF(
        QPoint(YtMAX(0, tTopLeft.x()), YtMAX(0, tTopLeft.y())),
        QPoint(YtMIN(tBottomRight.x() + 1, m_showimage.width()), YtMIN(tBottomRight.y() + 1, m_showimage.height())));

    QRgb pixValue;
    CMvPolygon SetPloy;
    int showWith = rect.width() / 8;
    int showhight = rect.height() / 8;
    ///////////
    m_OverPlayShow.m_DispLines.append(DispLines(CMvLine(impoint.x(), impoint.y(), 0), Qt::blue, 1, true));
    m_OverPlayShow.m_DispLines.append(DispLines(CMvLine(impoint.x(), impoint.y(), 90), Qt::blue, 1, true));
    for (int y = rect.y(); y < rect.y() + rect.height(); y++)
    {
        pixValue = m_showimage.pixel(impoint.x(), y);
        SetPloy.points.append(CMvPoint(impoint.x() + (qGray(pixValue) - 128) / 255.0 * showWith, y));
    }
    m_OverPlayShow.m_DispPolygonEs.append(DispPolygonEs(SetPloy, Qt::red, 1));
    SetPloy.toClearData();
    for (int x = rect.x(); x < rect.x() + rect.width(); x++)
    {
        pixValue = m_showimage.pixel(x, impoint.y());
        SetPloy.points.append(CMvPoint(x, impoint.y() + (qGray(pixValue) - 128) / 255.0 * showhight));
    }
    m_OverPlayShow.m_DispPolygonEs.append(DispPolygonEs(SetPloy, Qt::red, 1));
    m_scene->update();
}

void QGView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
    {
        GetFit();
    }
    QGraphicsView::mousePressEvent(event);
}

void QGView::resizeEvent(QResizeEvent *event)
{
    tedit->setGeometry(0, this->height() - 15, 1, 16);
    GetFit();
    thorizontalScrollBar->setValue(this->horizontalScrollBar()->value());
    tverticalScrollBar->setValue(this->verticalScrollBar()->value());
    QGraphicsView::resizeEvent(event);
}

void QGView::mouseReleaseEvent(QMouseEvent *event)
{

    //

    thorizontalScrollBar->setValue(this->horizontalScrollBar()->value());
    tverticalScrollBar->setValue(this->verticalScrollBar()->value());

    QGraphicsView::mouseReleaseEvent(event);
}

void QGView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (this->isFullScreen())
    {
        this->setWindowFlags(Qt::SubWindow);
        this->showNormal();
        GetFit();
        QGraphicsView::mouseDoubleClickEvent(event);

        return;
    }
    //
    if (event->button() == Qt::RightButton)
    {

        m_menu->popup(event->globalPos());
    }
    else if (event->button() == Qt::LeftButton)
    {
        if (m_RightType != 0)
        {
            auto item = this->mapToScene(event->pos());
            int X = item.x();
            int Y = item.y();
            emit m_YtRoiShowDisp->toLeftDoubleOut(QString("%1#%2").arg(X).arg(Y));
        }
        else
        {
            m_ZoomValue *= 1.3;
            m_ZoomValueInver = 1.0 / m_ZoomValue;
            ZoomFrame(m_ZoomValue);
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void QGView::mouseMoveEvent(QMouseEvent *event)
{
    this->setCursor(Qt::SizeAllCursor);

    auto item = this->mapToScene(event->pos());
    int X = item.x();
    int Y = item.y();
    if (X > -1 && X < m_showimage.width() && Y > -1 && Y < m_showimage.height())
    {
        QRgb tGetColor = m_showimage.pixel(X, Y);
        QString getshowStr;
        if (m_showimage.format() == QImage::Format_Indexed8 || m_showimage.format() == QImage::Format_Grayscale8)
        {
            getshowStr = QString(u8"W:%1 H:%2|x:%3,y:%4|Gray:%5")
                             .arg(m_showimage.width())
                             .arg(m_showimage.height())
                             .arg(X)
                             .arg(Y)
                             .arg(qGray(tGetColor));
        }
        else if (m_showimage.format() == QImage::Format_Grayscale16)
        {
            getshowStr = QString(u8"W:%1 H:%2|x:%3,y:%4|Gray:%5")
                             .arg(m_showimage.width())
                             .arg(m_showimage.height())
                             .arg(X)
                             .arg(Y)
                             .arg(*reinterpret_cast<const ushort *>(m_showimage.constBits() +
                                                                    (Y * m_showimage.width() + X) * 2));
        }
        else
        {
            getshowStr = QString(u8"W:%1 H:%2|x:%3,y:%4|R:%5,G:%6,B:%7")
                             .arg(m_showimage.width())
                             .arg(m_showimage.height())
                             .arg(X)
                             .arg(Y)
                             .arg(qRed(tGetColor))
                             .arg(qGreen(tGetColor))
                             .arg(qBlue(tGetColor));
        }
        tedit->setText(getshowStr);
        tedit->adjustSize();
        emit m_YtRoiShowDisp->toGetPixInfo(QPoint(X, Y), tGetColor, m_showimage.format());
        //尝试画两条线
        toDrawPrline(item, true);
    }
    else
    {
        toDrawPrline(item, false);
    }

    QGraphicsView::mouseMoveEvent(event);
}

void QGView::wheelEvent(QWheelEvent *event)
{
    m_ZoomValue = event->angleDelta().y() > 0 ? m_ZoomValue * 1.1 : m_ZoomValue * 0.9;
    m_ZoomValue = qMax(m_ZoomValue, 0.01);
    m_ZoomValue = qMin(m_ZoomValue, 100.0);
    m_ZoomValueInver = 1.0 / m_ZoomValue;
    ZoomFrame(m_ZoomValue);
    thorizontalScrollBar->setValue(this->horizontalScrollBar()->value());
    tverticalScrollBar->setValue(this->verticalScrollBar()->value());
}

void QGView::drawForeground(QPainter *painter, const QRectF &rect)
{
    Q_UNUSED(rect);
    Q_UNUSED(painter);
}

void QGView::drawBackground(QPainter *painter, const QRectF &rect)
{
    Q_UNUSED(rect);
    painter->drawImage(QPoint(), m_showimage); //绘制背景曾
    toPaintGridShow(painter);

    m_UpdateTime = QDateTime::currentDateTime();
    YtSetShowtObj temoveplay;
    //这里是显示常规图层
    foreach (QString SetKey, m_OverPlayItemMap.keys())
    {

        temoveplay.toClearData();
        temoveplay.AddByAnother((YtSetShowtObj *)m_OverPlayItemMap.value(SetKey));
        temoveplay.m_MaskIm = ((YtSetShowtObj *)m_OverPlayItemMap.value(SetKey))->m_MaskIm;

        // temoveplay.toPaint(painter,this->rect(),m_ZoomValue);
        QRect temrect = this->rect();
        temoveplay.toPaint(painter,
                           QRect(-temrect.width(), -temrect.height(), 2 * temrect.width(), 2 * temrect.height()),
                           m_ZoomValue);
    }
    QPainter paint(this->viewport());

    //这里是显示固定图层的
    foreach (QString SetKey, m_StdOverPlayItemMap.keys())
    {
        temoveplay.toClearData();
        temoveplay.AddByAnother((YtSetShowtObj *)m_StdOverPlayItemMap.value(SetKey));
        temoveplay.toPaintStd(&paint, this->rect());
    }
    toPaintGrayPixVal(&paint);
}

void QGView::paintEvent(QPaintEvent *event)
{
    QPainter paint(this->viewport());
    paint.fillRect(this->rect(), m_BackColor);
    QGraphicsView::paintEvent(event);
    paint.end();
}
