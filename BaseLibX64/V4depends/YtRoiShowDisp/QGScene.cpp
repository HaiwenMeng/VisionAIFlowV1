#include "QGScene.h"
#include <QGraphicsSceneMouseEvent>
#include <QLineEdit>
#include <QGraphicsProxyWidget>
#include "BaseItem.h"
//https://gitee.com/falion/qt-draw-region

QGScene::QGScene(QObject *parent) : QGraphicsScene(parent)
{
//https://gitee.com/falion/qt-draw-region
}


void QGScene::UpDateZoom(qreal val)
{
    for (auto Item:this->items())
    {
        //通过属性控制可以避免软件闪退，不然会刷的太快，直接赋值的话
        ((BaseItem*)Item)->setProperty("ZoomVal",val);
    }
}

