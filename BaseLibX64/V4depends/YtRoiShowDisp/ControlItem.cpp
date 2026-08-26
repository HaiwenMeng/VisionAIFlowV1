#include "ControlItem.h"
#include "BaseItem.h"
#include <QDebug>

ControlItem::ControlItem(QGraphicsItemGroup* parent, QPointF p, int nPointIndex, int nControlType)
    : QAbstractGraphicsShapeItem(parent)
    , m_pointPos(p)
    , m_nPointIndex(nPointIndex)
    , m_nControlType(nControlType)
{
    m_nItemSize = YtPointControlSize;
    this->setPos(m_pointPos);
    m_Pen = this->pen();
    m_Pen.setWidthF(1); // 缩放后线宽 0 为任何情况下1像素
    m_Pen.setColor(Qt::red);
    // 设置缓冲 需要配合 update 使用
    setCacheMode(DeviceCoordinateCache);
    // 设置只接受鼠标左键事件
    setAcceptedMouseButtons(Qt::LeftButton);

    // 非常关键 确保点击子元素不会丢失焦点
    this->setFlags(QGraphicsItem::ItemIsSelectable |
                   QGraphicsItem::ItemIsMovable |
                   QGraphicsItem::ItemIsFocusable);
    //    this->setFlags(QGraphicsItem::ItemIsSelectable |
    //                   QGraphicsItem::ItemIsMovable |
    //                   QGraphicsItem::ItemIsFocusable);

    switch (m_nControlType) {
    case Move_Control:
        this->setCursor(Qt::SizeAllCursor);
        break;
    case Size_Control:
        this->setCursor(Qt::SizeFDiagCursor);
        break;
    case Rotate_Control:
        this->setCursor(Qt::SizeFDiagCursor);
        break;
    default:
        break;
    }

    m_ItemPath.addRect(QRectF(-m_nItemSize, -m_nItemSize, m_nItemSize * 2, m_nItemSize * 2));
}

QPointF ControlItem::GetPoint()
{
    return m_pointPos;
}
void ControlItem::SetPoint(QPointF p)
{
    m_pointPos = p;
    this->setPos(p);
}

QRectF ControlItem::boundingRect() const
{
    return m_ItemPath.boundingRect(); //拖拽 鼠标感应区域
}

void ControlItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    //位置重绘
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setPen(m_Pen);
    this->setPos(m_pointPos);

    switch (m_nControlType) {
    case Move_Control:
        painter->setBrush(QBrush(Qt::white));
        painter->drawEllipse(m_ItemPath.boundingRect());
        break;
    case Size_Control:
        painter->setBrush(QBrush(Qt::white));
        painter->drawRect(m_ItemPath.boundingRect());
        break;
    case Rotate_Control:
        painter->setBrush(QBrush(Qt::darkYellow));
        painter->drawRect(m_ItemPath.boundingRect());
        break;
    default:
        break;
    }

    // 显示控制点编号
    //QFont font;
    //font.setPointSizeF(8);
    //font.setBold(true);
    //painter->setFont(font);
    //painter->setPen(Qt::red);
    //painter->drawText(QPointF(-4, 4), QString::number(m_nPointIndex));
}

void ControlItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->buttons() == Qt::LeftButton)
    {
        m_dx = event->scenePos().x() - event->lastScenePos().x();
        m_dy = event->scenePos().y() - event->lastScenePos().y();

        BaseItem *pBaseItem = static_cast<BaseItem *>(this->parentItem());
        if(m_nControlType == Move_Control)
        {
            pBaseItem->moveBy(m_dx, m_dy);
        }
        else
        {
            //记录上一次座标结果
            m_BefPoint = m_pointPos;
            m_pointPos = this->mapToParent(event->pos());
            //更新结果
            bool flg = pBaseItem->UpDate(m_nPointIndex);
            if(flg) {
                //结果正常、更新位置
                this->setPos(m_pointPos);
            }
            else {
                //结果异常、退回上一次的位置
                m_pointPos = m_BefPoint;
                this->setPos(m_pointPos);
            }
        }
    }
}

double ControlItem::dX()
{
    return m_dx;
}

double ControlItem::dY()
{
    return m_dy;
}
