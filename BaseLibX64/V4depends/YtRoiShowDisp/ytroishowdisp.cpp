#include "ytroishowdisp.h"
#include "ui_ytroishowdisp.h"
#include <QGScene.h>
YtRoiShowDisp::YtRoiShowDisp(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::YtRoiShowDisp)
{
    ui->setupUi(this);
    ui->graphicsView->m_YtRoiShowDisp=this;
    ui->graphicsView->m_QScrollBar=ui->verticalScrollBar;
    ui->verticalScrollBar->hide();
    ui->horizontalScrollBar->hide();
    connect(ui->verticalScrollBar,&QScrollBar::sliderMoved,ui->graphicsView,&QGView::toSetCurentView);
    connect(ui->horizontalScrollBar,&QScrollBar::sliderMoved,ui->graphicsView,&QGView::toSetCurentViewX);
    //
    ui->graphicsView->thorizontalScrollBar=ui->horizontalScrollBar;
    ui->graphicsView->tverticalScrollBar=ui->verticalScrollBar;
}

YtRoiShowDisp::~YtRoiShowDisp()
{
    delete ui;
}

void YtRoiShowDisp::addStdOverPlayPtr(void *tpart, QString key)
{
    if(!ui->graphicsView->m_StdOverPlayItemMap.contains(key))
    {
        ui->graphicsView->m_StdOverPlayItemMap.insert(key,tpart);
    }
}

void YtRoiShowDisp::removeStdOverPlayPtr(QString key)
{
    if(ui->graphicsView->m_StdOverPlayItemMap.contains(key))
    {
        ui->graphicsView->m_StdOverPlayItemMap.remove(key);
    }
}

void YtRoiShowDisp::ClearAllStdOverPlayPtr()
{
    ui->graphicsView->m_StdOverPlayItemMap.clear();
    ui->graphicsView->m_StdOverPlayItemMap.insert("INERRSHOW",& ui->graphicsView->m_StdOverPlayShow);
}

void YtRoiShowDisp::addOverPlayPtr(void *tpart, QString key)
{
    if(!ui->graphicsView->m_OverPlayItemMap.contains(key))
    {
        ui->graphicsView->m_OverPlayItemMap.insert(key,tpart);
    }

}

void YtRoiShowDisp::removeOverPlayPtr(QString key)
{
    if(ui->graphicsView->m_OverPlayItemMap.contains(key))
    {
        ui->graphicsView->m_OverPlayItemMap.remove(key);
    }
}

void YtRoiShowDisp::ClearAllOverPlayPtr()
{
    ui->graphicsView->m_OverPlayItemMap.clear();
    ui->graphicsView->m_OverPlayItemMap.insert("INERRSHOW",&ui->graphicsView->m_OverPlayShow);

}

void YtRoiShowDisp::toViewPoint(double centerx, double centery, double scale)
{
    ui->graphicsView->toViewPoint(centerx,centery,scale);
}

void YtRoiShowDisp::toSetBGColor(QColor color)
{
    ui->graphicsView->m_BackColor=color;
    ui->graphicsView->update();
}

void YtRoiShowDisp::toSetImage(QImage showim)
{

    ui->graphicsView->toDispimage(showim);
    ui->verticalScrollBar->setRange(-showim.height(),showim.height());
    ui->horizontalScrollBar->setRange(-showim.width(),showim.width());


}

void YtRoiShowDisp::toSetImageData(unsigned char *imagedata, int imwidth, int imhieht, int chanle)
{
    if(ui->graphicsView->m_showimage.width()!=imwidth||ui->graphicsView->m_showimage.height()!=imhieht)
    {
        if(chanle==1)
        {
            ui->graphicsView->m_showimage=QImage(imwidth,imhieht,QImage::Format_Grayscale8);
        }
        else if(chanle==3)
        {
            ui->graphicsView->m_showimage=QImage(imwidth,imhieht,QImage::Format_RGB888);
        }
        else
        {
            return;
        }
    }
    memcpy(ui->graphicsView->m_showimage.bits(),imagedata,imwidth*imhieht*chanle);
    ui->graphicsView->GetFit();
    ui->verticalScrollBar->setRange(-imhieht,imhieht);
    ui->horizontalScrollBar->setRange(-imwidth,imwidth);


}

void YtRoiShowDisp::toLoadFileImage(QString filename)
{
    ui->graphicsView->m_showimage.load(filename);
    ui->verticalScrollBar->setRange(-ui->graphicsView->m_showimage.height(),ui->graphicsView->m_showimage.height());
    ui->horizontalScrollBar->setRange(-ui->graphicsView->m_showimage.width(),ui->graphicsView->m_showimage.width());

    ui->graphicsView->GetFit();
}

void YtRoiShowDisp::toUpdateShow(bool isfitshow)
{
    if(isfitshow)
    {
        ui->graphicsView->GetFit();
    }
//    if(ui->graphicsView->m_UpdateTime.addMSecs(100)<QDateTime::currentDateTime())
//    {
//        ui->graphicsView->m_UpdateTime=QDateTime::currentDateTime();
        ui->graphicsView->m_scene->update();
//    }
}

void YtRoiShowDisp::toSetLeftDoubleMode(int t, QString key)
{
    ui->graphicsView->m_IDKey=key;
    ui->graphicsView->m_RightType=t;//右键功能
}

void YtRoiShowDisp::addROI(int shape, QVector<double> Val, QString key)
{
    ui->graphicsView->addROI(shape,Val,key);
    toUpdateShow();
}

QVector<double> YtRoiShowDisp::getROI(QString key)
{
    return ui->graphicsView->getROI(key);
}

void YtRoiShowDisp::toRemoveRoiByKey(QString key)
{

    ui->graphicsView->toRemoveRoiByKey(key);
}

void YtRoiShowDisp::toRemoveAllRoi()
{

    ui->graphicsView->toRemoveAllRoi();
}

void YtRoiShowDisp::toClearInnerOverPlay()
{
    ui->graphicsView->m_OverPlayShow.toClearData();
    ui->graphicsView->m_OverPlayShow.toClearMask();

}

void YtRoiShowDisp::toClearInnerStdOverPlay()
{
    ui->graphicsView->m_StdOverPlayShow.toClearData();
    ui->graphicsView->m_StdOverPlayShow.toClearMask();
}

void YtRoiShowDisp::toApendInnerOverPlay(void *StdObj)
{

    if(StdObj)
    {
        ui->graphicsView->m_OverPlayShow.AddByAnother((YtSetShowtObj *)StdObj);
    }

}

void YtRoiShowDisp::toApendInnerStdOverPlay(void *StdObj)
{
    if(StdObj)
    {
        ui->graphicsView->m_StdOverPlayShow.AddByAnother((YtSetShowtObj *)StdObj);
    }
}

void YtRoiShowDisp::toSetLineShow(int row, int col, int linewidth, QColor LineColor)
{
    ui->graphicsView->m_Setrow=row;
    ui->graphicsView->m_Setcol=col;
    ui->graphicsView->m_Setlinewidth=linewidth;
    ui->graphicsView->m_SetLineColor=LineColor;
    toUpdateShow();

}

QRect YtRoiShowDisp::toGetViewIm()
{
    return ui->graphicsView->toGetViewIm();
}

QImage YtRoiShowDisp::toGetShotCut()
{
    QPixmap  pixmap;
    pixmap=this->grab();
    return pixmap.toImage();
}

void YtRoiShowDisp::toSetSliderView(bool istrue)
{
    if(istrue)
    {
        //

        //
        ui->verticalScrollBar->show();
        ui->horizontalScrollBar->show();

    }
    else
    {
        ui->verticalScrollBar->hide();
        ui->horizontalScrollBar->hide();


    }
}

QImage YtRoiShowDisp::toGetImPaint(double GetScal)
{
    QSize getSize=ui->graphicsView->m_showimage.size();
    QImage CopIm=ui->graphicsView->m_showimage.convertToFormat(QImage::Format_RGB888);
    QPainter painter(&CopIm);
    YtSetShowtObj *temoveplay;
    //    YtSetShowtObj m_OverPlayShow;//图层显示
    //    YtSetShowtObj m_StdOverPlayShow;//固定图层显示
    ui->graphicsView->m_OverPlayShow.toPaint(&painter,ui->graphicsView->m_showimage.rect(),1.0/GetScal);
    ui->graphicsView->m_StdOverPlayShow.toPaint(&painter,ui->graphicsView->m_showimage.rect(),1.0/GetScal);

    ui->graphicsView->toPaintGridShow(&painter);
    //这里是显示常规图层
    foreach (QString SetKey, ui->graphicsView->m_OverPlayItemMap.keys())
    {

        temoveplay=(YtSetShowtObj *)ui->graphicsView->m_OverPlayItemMap.value(SetKey);
        temoveplay->toPaint(&painter,ui->graphicsView->m_showimage.rect(),1.0/GetScal);
    }
    //这里是显示固定图层的
    foreach (QString SetKey, ui->graphicsView->m_StdOverPlayItemMap.keys())
    {
        temoveplay=(YtSetShowtObj *)ui->graphicsView->m_StdOverPlayItemMap.value(SetKey);
        temoveplay->toPaint(&painter,ui->graphicsView->m_showimage.rect(),1.0/GetScal);
    }
    return CopIm.scaled(GetScal*getSize.width(),GetScal*getSize.height());
}
