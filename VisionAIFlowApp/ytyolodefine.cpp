#include "ytyolodefine.h"

#include <QStringConverter>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QJsonObject>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QTextCodec>
#include "opencv2/opencv.hpp"

CMvRect toGetPointS(QVector<CMvPoint> GetPos)
{
    if (GetPos.size() < 2)
    {
        return CMvRect(0, 0, 0, 0);
    }
    CMvPoint TLPos, BRPos;
    TLPos = GetPos[0];
    BRPos = GetPos[0];
    //
    for (int i = 1; i < GetPos.size(); i++)
    {
        TLPos.x = qMin(TLPos.x, GetPos[i].x);
        TLPos.y = qMin(TLPos.y, GetPos[i].y);
        BRPos.x = qMax(BRPos.x, GetPos[i].x);
        BRPos.y = qMax(BRPos.y, GetPos[i].y);
    }
    return CMvRect(TLPos.x, TLPos.y, BRPos.x - TLPos.x, BRPos.y - TLPos.y);
}

YtRoiLabelSet::YtRoiLabelSet()
{
}

void YtRoiLabelSet::toSaveJson(QString Path)
{

    qDebug() << "YtRoiLabelSet::toSaveJson(QString Path)" << m_imagePath << Path << m_SetLabeset.size();
    // 保存就要给路径
    int getindex = m_imagePath.lastIndexOf(".");
    if (getindex < 0)
    {
        return;
    }
    if (m_SetLabeset.size() < 1)
    {
        QFile::remove(QString("%1/%2.json").arg(Path).arg(m_imagePath.left(getindex)));
        return;
    }
    QFile file(QString("%1/%2.json").arg(Path).arg(m_imagePath.left(getindex)));
    qDebug() << Path << file.fileName();
    if (!file.open(QIODevice::WriteOnly))
    {

        return;
    }

    // 创建一个JSON对象
    QJsonObject jsonObject;

    QJsonArray getlist;
    for (int i = 0; i < m_SetLabeset.size(); i++)
    {
        LabelSet temp = m_SetLabeset.at(i);
        QJsonObject tjsonObject;
        tjsonObject.insert("shape_type", temp.shape_type);
        tjsonObject.insert("label", temp.Name);
        if (temp.shape_type == "rotation")
        {
            CMvRotatedRect temdat;
            temdat.GetData(temp.toGetRoiData());
            tjsonObject.insert("direction", (360 - temdat.Deg()) / 180 * MV_PI);
        }
        QVector<QPointF> temgetposint = temp.toGetPoints();
        // 存储点集
        QJsonArray pointss;
        for (int j = 0; j < temgetposint.size(); j++)
        {
            QJsonArray OnePos;
            OnePos.append(temgetposint.at(j).x());
            OnePos.append(temgetposint.at(j).y());
            pointss.append(OnePos);
        }
        tjsonObject.insert("points", pointss);
        //
        getlist.append(tjsonObject);
    }
    jsonObject.insert("shapes", getlist);

    //
    jsonObject.insert("imagePath", m_imagePath);
    jsonObject.insert("imageWidth", m_imWidth);
    jsonObject.insert("imageHeight", m_imHight);
    jsonObject.insert("imageData", QJsonValue());
    // 创建一个JSON文档
    QJsonDocument jsonDoc(jsonObject);
    // 写入到文件
    file.write(jsonDoc.toJson(QJsonDocument::Indented));
    file.close();
}

void YtRoiLabelSet::toLoadJson(QString Path)
{
    m_NameSetLis.clear();
    m_SetLabeset.clear();
    // 读取文件到QByteArray
    int getindex = m_imagePath.lastIndexOf(".");
    if (getindex < 0)
    {
        return;
    }

    QFile file(QString("%1/%2.json").arg(Path).arg(m_imagePath.left(getindex)));
    qDebug() << "load json:" << Path << file.fileName();
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "ZZZ";
        return;
    }
    QByteArray jsonData = file.readAll();
    file.close();

    // 从QByteArray解析JSON文档
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);

    // 确保JSON文档是一个对象
    if (!jsonDoc.isObject())
    {
        return;
    }
    //
    QJsonObject jsonObject = jsonDoc.object();
    //
    //

    QString value1 = jsonObject["imagePath"].toString();
    m_imagePath = value1;
    //
    m_imWidth = jsonObject["imageWidth"].toInt();
    m_imHight = jsonObject["imageHeight"].toInt();
    //
    QJsonArray getlist = jsonObject["shapes"].toArray();
    //
    // qDebug()<<getlist<<getlist.size();

    for (int i = 0; i < getlist.size(); i++)
    {
        QJsonObject tjsonObject = getlist[i].toObject();

        QString shape = tjsonObject["shape_type"].toString();
        QString LabelName = tjsonObject["label"].toString();
        double direction = tjsonObject["direction"].toDouble();
        QJsonArray pointss = tjsonObject["points"].toArray();
        if (!m_NameSetLis.contains(LabelName))
        {
            m_NameSetLis << LabelName;
        }

        // qDebug()<<shape<<LabelName<<i;
        QVector<QPointF> tempoints;
        for (int j = 0; j < pointss.size(); j++)
        {
            QJsonArray getAtt = pointss[j].toArray();
            // qDebug()<<pointss[j]<<getAtt[0].toDouble()<<getAtt[1].toDouble()<<j;
            tempoints.append(QPointF(getAtt[0].toDouble(), getAtt[1].toDouble()));
        }
        //
        LabelSet temset;
        // qDebug()<<i<<shape<<tempoints.size()<<direction<<"toInitData";
        if (temset.toInitData(LabelName, shape, tempoints, direction))
        {
            //
            m_SetLabeset.append(temset);
        }
    }
}

void YtRoiLabelSet::toLoadJsonFile(QString FileName)
{
    m_NameSetLis.clear();
    m_SetLabeset.clear();
    // 读取文件到QByteArray

    QFile file(FileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        return;
    }
    QByteArray jsonData = file.readAll();
    file.close();

    // 从QByteArray解析JSON文档
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);

    // 确保JSON文档是一个对象
    if (!jsonDoc.isObject())
    {
        return;
    }
    //
    QJsonObject jsonObject = jsonDoc.object();
    //
    //

    QString value1 = jsonObject["imagePath"].toString();
    m_imagePath = value1;
    //
    m_imWidth = jsonObject["imageWidth"].toInt();
    m_imHight = jsonObject["imageHeight"].toInt();
    //
    QJsonArray getlist = jsonObject["shapes"].toArray();
    //
    // qDebug()<<getlist<<getlist.size();

    for (int i = 0; i < getlist.size(); i++)
    {
        QJsonObject tjsonObject = getlist[i].toObject();

        QString shape = tjsonObject["shape_type"].toString();
        QString LabelName = tjsonObject["label"].toString();
        double direction = tjsonObject["direction"].toDouble();
        QJsonArray pointss = tjsonObject["points"].toArray();
        if (!m_NameSetLis.contains(LabelName))
        {
            m_NameSetLis << LabelName;
        }

        // qDebug()<<shape<<LabelName<<i;
        QVector<QPointF> tempoints;
        for (int j = 0; j < pointss.size(); j++)
        {
            QJsonArray getAtt = pointss[j].toArray();
            // qDebug()<<pointss[j]<<getAtt[0].toDouble()<<getAtt[1].toDouble()<<j;
            tempoints.append(QPointF(getAtt[0].toDouble(), getAtt[1].toDouble()));
        }
        //
        LabelSet temset;
        // qDebug()<<i<<shape<<tempoints.size()<<direction<<"toInitData";
        if (temset.toInitData(LabelName, shape, tempoints, direction))
        {
            //
            m_SetLabeset.append(temset);
        }
    }
}

void YtRoiLabelSet::toAppendData(LabelSet LabelName)
{
    m_SetLabeset.append(LabelName);
    if (!m_NameSetLis.contains(LabelName.Name))
    {
        m_NameSetLis << LabelName.Name;
    }
}

void YtRoiLabelSet::toInserData(LabelSet LabelName, int index)
{
    m_SetLabeset.insert(index, LabelName);
    if (!m_NameSetLis.contains(LabelName.Name))
    {
        m_NameSetLis << LabelName.Name;
    }
}

void YtRoiLabelSet::toModify(LabelSet LabelName, int setindex, bool ischangename)
{
    if (setindex >= m_SetLabeset.size() || setindex < 0)
    {
        return;
    }
    m_SetLabeset[setindex] = LabelName;
    if (ischangename)
    {
        m_NameSetLis.clear();

        for (int i = 0; i < m_SetLabeset.size(); i++)
        {
            if (!m_NameSetLis.contains(m_SetLabeset.at(i).Name))
            {
                m_NameSetLis << m_SetLabeset.at(i).Name;
            }
        }
    }
}

void YtRoiLabelSet::toReMoveIndex(int setindex)
{
    m_SetLabeset.remove(setindex);
    m_NameSetLis.clear();

    for (int i = 0; i < m_SetLabeset.size(); i++)
    {
        if (!m_NameSetLis.contains(m_SetLabeset.at(i).Name))
        {
            m_NameSetLis << m_SetLabeset.at(i).Name;
        }
    }
}

void YtRoiLabelSet::toClearData()
{
    m_SetLabeset.clear();
    m_NameSetLis.clear();
}

void YtRoiLabelSet::toMovePos(int xPos, int yPos)
{
    for (int i = 0; i < m_SetLabeset.size(); i++)
    {
        // 在这里逐行数据书写
        LabelSet temp = m_SetLabeset.at(i);
        switch (temp.toGetRoiType())
        {
        case LabelSet::LrectangleROI:
        {
            CMvRect temdata;
            temdata.GetData(temp.toGetRoiData());
            // 获取四个点
            temdata.LeftTop.x += xPos;
            temdata.LeftTop.y += yPos;
            temp.toInitProData(temp.Name, LabelSet::LrectangleROI, temdata.Data());

            toModify(temp, i);

            break;
        }
        case LabelSet::LrotaterectangleROI:
        {
            CMvRotatedRect temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            temdata.Center.x += xPos;
            temdata.Center.y += yPos;
            temp.toInitProData(temp.Name, LabelSet::LrotaterectangleROI, temdata.Data());

            toModify(temp, i);
            break;
        }
        case LabelSet::LpointROI:
        {
            CMvPoint temdata;
            temdata.GetData(temp.toGetRoiData());

            temdata.x += xPos;
            temdata.y += yPos;
            temp.toInitProData(temp.Name, LabelSet::LpointROI, temdata.Data());

            toModify(temp, i);
            break;
        }
        case LabelSet::LpolygonROI:
        {
            CMvPolygon temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            for (int j = 0; j < temdata.points.size(); i++)
            {
                temdata.points[j].x += xPos;
                temdata.points[j].y += yPos;
            }
            temp.toInitProData(temp.Name, LabelSet::LpolygonROI, temdata.Data());
            toModify(temp, i);

            break;
        }
        case LabelSet::LcircleROI:
        {
            CMvCircle temdata;
            temdata.GetData(temp.toGetRoiData());

            temdata.center.x += xPos;
            temdata.center.y += yPos;
            temp.toInitProData(temp.Name, LabelSet::LcircleROI, temdata.Data());

            toModify(temp, i);

            break;
        }
        case LabelSet::LlineSegROI:
        {
            CMvLineSeg temdata;
            temdata.GetData(temp.toGetRoiData());

            temdata.st.x += xPos;
            temdata.st.y += yPos;
            temdata.ed.x += xPos;
            temdata.ed.y += yPos;
            temp.toInitProData(temp.Name, LabelSet::LlineSegROI, temdata.Data());

            toModify(temp, i);

            break;
        }
        }
    }
}

void YtRoiLabelSet::toSaveDetcTxt(QString Path, QVector<QString> NameList, QSize CropSize)
{
    // 保存就要给路径
    int getindex = m_imagePath.lastIndexOf(".");
    if (getindex < 0)
    {
        return;
    }

    QFile file(QString("%1/%2.txt").arg(Path).arg(m_imagePath.left(getindex)));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }
    QTextStream s(&file);
    QString temstr;
    // 创建一个JSON对象
    for (int i = 0; i < m_SetLabeset.size(); i++)
    {
        // 在这里逐行数据书写
        LabelSet temp = m_SetLabeset.at(i);
        int GetIndex = NameList.indexOf(temp.Name);
        if (GetIndex < 0)
        {
            continue;
        }
        switch (temp.toGetRoiType())
        {
        case LabelSet::LrectangleROI:
        {
            CMvRect temdata;
            temdata.GetData(temp.toGetRoiData());
            // 获取四个点
            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       (temdata.LeftTop.x + temdata.cx / 2) / m_imWidth,
                                       (temdata.LeftTop.y + temdata.cy / 2) / m_imHight,
                                       (temdata.cx) / m_imWidth,
                                       (temdata.cy) / m_imHight);
            s << temstr << "\n";

            break;
        }
        case LabelSet::LrotaterectangleROI:
        {
            CMvRotatedRect temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            QVector<CMvPoint> Setpoints;
            CMvPoint getpos[4];
            temdata.toGetPoint(getpos);
            Setpoints << getpos[0] << getpos[1] << getpos[2] << getpos[3];
            //
            CMvRect temrct = toGetPointS(Setpoints);
            //
            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       (temrct.LeftTop.x + temdata.cx / 2) / m_imWidth,
                                       (temrct.LeftTop.y + temdata.cy / 2) / m_imHight,
                                       (temrct.cx) / m_imWidth,
                                       (temrct.cy) / m_imHight);
            s << temstr << "\n";
            break;
        }
        case LabelSet::LpointROI:
        {
            CMvPoint temdata;
            temdata.GetData(temp.toGetRoiData());

            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       temdata.x / m_imWidth,
                                       temdata.y / m_imHight,
                                       1.0 * CropSize.width() / m_imWidth,
                                       1.0 * CropSize.height() / m_imHight);
            s << temstr << "\n";
            break;
        }
        case LabelSet::LpolygonROI:
        {
            CMvPolygon temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            CMvRect temrct = toGetPointS(temdata.points);
            //
            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       (temrct.LeftTop.x + temrct.cx / 2) / m_imWidth,
                                       (temrct.LeftTop.y + temrct.cy / 2) / m_imHight,
                                       (temrct.cx) / m_imWidth,
                                       (temrct.cy) / m_imHight);
            s << temstr << "\n";

            break;
        }
        case LabelSet::LcircleROI:
        {
            CMvCircle temdata;
            temdata.GetData(temp.toGetRoiData());

            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       temdata.center.x / m_imWidth,
                                       temdata.center.y / m_imHight,
                                       2.0 * temdata.radius / m_imWidth,
                                       2.0 * temdata.radius / m_imHight);
            s << temstr << "\n";

            break;
        }
        case LabelSet::LlineSegROI:
        {
            CMvLineSeg temdata;
            temdata.GetData(temp.toGetRoiData());

            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       temdata.st.x / m_imWidth,
                                       temdata.st.y / m_imHight,
                                       temdata.ed.x / m_imWidth,
                                       temdata.ed.y / m_imHight);
            s << temstr << "\n";

            break;
        }
        }
    }
    file.close();
}

void YtRoiLabelSet::toGenDetcOveplay(YtSetShowtObj &setshow,
                                     QVector<QString> NameList,
                                     QVector<QColor> ColorLis,
                                     QSize CropSize)
{
    setshow.toClearData();
    // 创建一个JSON对象
    for (int i = 0; i < m_SetLabeset.size(); i++)
    {
        // 在这里逐行数据书写
        LabelSet temp = m_SetLabeset.at(i);
        int GetIndex = NameList.indexOf(temp.Name);
        if (GetIndex < 0)
        {
            continue;
        }
        switch (temp.toGetRoiType())
        {
        case LabelSet::LrectangleROI:
        {
            CMvRect temdata;
            temdata.GetData(temp.toGetRoiData());
            // 获取四个点
            setshow.append(DispRects(temdata, ColorLis.at(GetIndex), 1));
            setshow.append(DispTxt(temp.Name,
                                   CMvPoint(temdata.LeftTop.x, temdata.LeftTop.y - 13),
                                   QFont("Times", 13, 14),
                                   Qt::blue));

            break;
        }
        case LabelSet::LrotaterectangleROI:
        {
            CMvRotatedRect temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            QVector<CMvPoint> Setpoints;
            CMvPoint getpos[4];
            temdata.toGetPoint(getpos);
            Setpoints << getpos[0] << getpos[1] << getpos[2] << getpos[3];
            //
            CMvRect temrct = toGetPointS(Setpoints);
            setshow.append(DispRects(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(DispTxt(temp.Name,
                                   CMvPoint(temrct.LeftTop.x, temrct.LeftTop.y - 13),
                                   QFont("Times", 13, 14),
                                   Qt::blue));

            break;
        }
        case LabelSet::LpointROI:
        {
            CMvPoint temdata;
            temdata.GetData(temp.toGetRoiData());
            CMvRect temrct = CMvRect(temdata.x - CropSize.width() / 2,
                                     temdata.y - CropSize.height() / 2,
                                     CropSize.width(),
                                     CropSize.height());
            setshow.append(DispRects(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(DispTxt(temp.Name,
                                   CMvPoint(temrct.LeftTop.x, temrct.LeftTop.y - 13),
                                   QFont("Times", 13, 14),
                                   Qt::blue));
            break;
        }
        case LabelSet::LpolygonROI:
        {
            CMvPolygon temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            CMvRect temrct = toGetPointS(temdata.points);
            setshow.append(DispRects(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(DispTxt(temp.Name,
                                   CMvPoint(temrct.LeftTop.x, temrct.LeftTop.y - 13),
                                   QFont("Times", 13, 14),
                                   Qt::blue));
            break;
        }
        case LabelSet::LcircleROI:
        {
            CMvCircle temdata;
            temdata.GetData(temp.toGetRoiData());
            CMvRect temrct = CMvRect(temdata.center.x - temdata.radius,
                                     temdata.center.y - temdata.radius,
                                     2 * temdata.radius,
                                     2 * temdata.radius);
            setshow.append(DispRects(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(DispTxt(temp.Name,
                                   CMvPoint(temrct.LeftTop.x, temrct.LeftTop.y - 13),
                                   QFont("Times", 13, 14),
                                   Qt::blue));
            break;
        }
        case LabelSet::LlineSegROI:
        {
            CMvLineSeg temdata;
            temdata.GetData(temp.toGetRoiData());

            setshow.append(DispLineSegs(temdata, ColorLis.at(GetIndex), 1));
            setshow.append(
                DispTxt(temp.Name, CMvPoint(temdata.CenterX(), temdata.CenterY()), QFont("Times", 13, 14), Qt::blue));
            break;
        }
        }
    }
}

void YtRoiLabelSet::toLoadDetcTxt(QString filename, QSize imsize, QVector<QString> NameList)
{

    qDebug() << "toLoadDetcTxt";
    int getindex = filename.lastIndexOf(".");
    if (getindex < 0)
    {
        return;
    }
    toClearData();
    QFile file(QString("%1.txt").arg(filename.left(getindex)));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }
    //
    toSetImSize(imsize);
    QFileInfo temi(filename);
    toSetFileName(temi.fileName());

    QTextStream in(&file);
    QStringList Getlist;
    LabelSet temp;
    CMvRect temdata;
    while (!in.atEnd())
    {
        QString line = in.readLine();
        // 处理每行文本
        Getlist = line.split(" ");
        if (Getlist.size() < 5)
        {
            continue;
        }
        int getNameindex = QString(Getlist[0]).toInt();
        if (getNameindex >= NameList.size())
        {
            continue;
        }
        double CenterX = QString(Getlist[1]).toDouble() * imsize.width();
        double CenterY = QString(Getlist[2]).toDouble() * imsize.height();
        double Width = QString(Getlist[3]).toDouble() * imsize.width();
        double Height = QString(Getlist[4]).toDouble() * imsize.height();
        //
        temdata.LeftTop.x = CenterX - Width / 2;
        temdata.LeftTop.y = CenterY - Height / 2;
        temdata.cx = Width;
        temdata.cy = Height;
        //
        temp.toInitProData(NameList.at(getNameindex), LabelSet::LrectangleROI, temdata.Data());
        toAppendData(temp);
    }

    file.close();
}

void YtRoiLabelSet::toSaveObbTxt(QString Path, QVector<QString> NameList, QSize CropSize)
{

    // 就是四个端点的位置

    // 保存就要给路径
    int getindex = m_imagePath.lastIndexOf(".");
    if (getindex < 0)
    {
        return;
    }

    QFile file(QString("%1/%2.txt").arg(Path).arg(m_imagePath.left(getindex)));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }
    QTextStream s(&file);
    QString temstr;
    // 创建一个JSON对象
    for (int i = 0; i < m_SetLabeset.size(); i++)
    {
        // 在这里逐行数据书写
        LabelSet temp = m_SetLabeset.at(i);
        int GetIndex = NameList.indexOf(temp.Name);
        if (GetIndex < 0)
        {
            continue;
        }
        switch (temp.toGetRoiType())
        {
        case LabelSet::LrectangleROI:
        {
            CMvRect temdata;
            temdata.GetData(temp.toGetRoiData());
            // 获取四个点
            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       temdata.LeftTop.x / m_imWidth,
                                       temdata.LeftTop.y / m_imHight,
                                       (temdata.LeftTop.x + temdata.cx) / m_imWidth,
                                       (temdata.LeftTop.y) / m_imHight,
                                       (temdata.LeftTop.x + temdata.cx) / m_imWidth,
                                       (temdata.LeftTop.y + temdata.cy) / m_imHight,
                                       (temdata.LeftTop.x) / m_imWidth,
                                       (temdata.LeftTop.y + temdata.cy) / m_imHight);
            s << temstr << "\n";

            break;
        }
        case LabelSet::LrotaterectangleROI:
        {
            CMvRotatedRect temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            QVector<CMvPoint> Setpoints;
            CMvPoint getpos[4];
            temdata.toGetPoint(getpos);
            Setpoints << getpos[0] << getpos[1] << getpos[2] << getpos[3];
            //
            // CMvRect temrct=toGetPointS(Setpoints);
            //
            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       getpos[0].x / m_imWidth,
                                       getpos[0].y / m_imHight,
                                       getpos[1].x / m_imWidth,
                                       getpos[1].y / m_imHight,
                                       getpos[2].x / m_imWidth,
                                       getpos[2].y / m_imHight,
                                       getpos[3].x / m_imWidth,
                                       getpos[3].y / m_imHight);
            s << temstr << "\n";
            break;
        }
        case LabelSet::LpointROI:
        {
            CMvPoint temdata;
            temdata.GetData(temp.toGetRoiData());

            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       (temdata.x - 0.5 * CropSize.width()) / m_imWidth,
                                       (temdata.y - 0.5 * CropSize.height()) / m_imHight,
                                       (temdata.x + 0.5 * CropSize.width()) / m_imWidth,
                                       (temdata.y - 0.5 * CropSize.height()) / m_imHight,
                                       (temdata.x + 0.5 * CropSize.width()) / m_imWidth,
                                       (temdata.y + 0.5 * CropSize.height()) / m_imHight,
                                       (temdata.x - 0.5 * CropSize.width()) / m_imWidth,
                                       (temdata.y + 0.5 * CropSize.height()) / m_imHight);
            s << temstr << "\n";
            break;
        }
        case LabelSet::LpolygonROI:
        {
            CMvPolygon temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            CMvRotatedRect temrct = toGetPRota(temdata.points);
            CMvPoint GetPoints[4];
            temrct.toGetPoint(GetPoints);

            //
            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       GetPoints[0].x / m_imWidth,
                                       GetPoints[0].y / m_imHight,
                                       GetPoints[1].x / m_imWidth,
                                       GetPoints[1].y / m_imHight,
                                       GetPoints[2].x / m_imWidth,
                                       GetPoints[2].y / m_imHight,
                                       GetPoints[3].x / m_imWidth,
                                       GetPoints[3].y / m_imHight);
            s << temstr << "\n";

            break;
        }
        case LabelSet::LcircleROI:
        {
            CMvCircle temdata;
            temdata.GetData(temp.toGetRoiData());

            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       (temdata.center.x - temdata.radius) / m_imWidth,
                                       (temdata.center.y - temdata.radius) / m_imHight,
                                       (temdata.center.x + temdata.radius) / m_imWidth,
                                       (temdata.center.y - temdata.radius) / m_imHight,
                                       (temdata.center.x + temdata.radius) / m_imWidth,
                                       (temdata.center.y + temdata.radius) / m_imHight,
                                       (temdata.center.x - temdata.radius) / m_imWidth,
                                       (temdata.center.y + temdata.radius) / m_imHight);
            s << temstr << "\n";

            break;
        }
        }
    }
    file.close();
}

void YtRoiLabelSet::toGenObbOveplay(YtSetShowtObj &setshow,
                                    QVector<QString> NameList,
                                    QVector<QColor> ColorLis,
                                    QSize CropSize)
{
    setshow.toClearData();
    // 创建一个JSON对象
    for (int i = 0; i < m_SetLabeset.size(); i++)
    {
        // 在这里逐行数据书写
        LabelSet temp = m_SetLabeset.at(i);
        int GetIndex = NameList.indexOf(temp.Name);
        if (GetIndex < 0)
        {
            continue;
        }
        switch (temp.toGetRoiType())
        {
        case LabelSet::LrectangleROI:
        {
            CMvRect temdata;
            temdata.GetData(temp.toGetRoiData());
            // 获取四个点
            CMvPolygon temrct;
            temrct.points.append(CMvPoint(temdata.LeftTop.x, temdata.LeftTop.y));
            temrct.points.append(CMvPoint(temdata.LeftTop.x + temdata.cx, temdata.LeftTop.y));
            temrct.points.append(CMvPoint(temdata.LeftTop.x + temdata.cx, temdata.LeftTop.y + temdata.cy));
            temrct.points.append(CMvPoint(temdata.LeftTop.x, temdata.LeftTop.y + temdata.cy));
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(LabTxt(temp.Name,
                                  CMvPoint(temdata.LeftTop.x + temdata.cx / 2, temdata.LeftTop.y + temdata.cy / 2),
                                  QFont("Times", 20, 14),
                                  Qt::white,
                                  0,
                                  Qt::blue));

            break;
        }
        case LabelSet::LrotaterectangleROI:
        {
            CMvRotatedRect temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            QVector<CMvPoint> Setpoints;
            CMvPoint getpos[4];
            temdata.toGetPoint(getpos);
            Setpoints << getpos[0] << getpos[1] << getpos[2] << getpos[3];
            //
            //
            CMvPolygon temrct;
            temrct.points.append(getpos[0]);
            temrct.points.append(getpos[1]);
            temrct.points.append(getpos[2]);
            temrct.points.append(getpos[3]);
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(LabTxt(temp.Name,
                                  CMvPoint(temdata.Center.x, temdata.Center.y),
                                  QFont("Times", 13, 14),
                                  Qt::white,
                                  0,
                                  Qt::blue));

            break;
        }
        case LabelSet::LpointROI:
        {
            CMvPoint temdata;
            temdata.GetData(temp.toGetRoiData());

            CMvPolygon temrct;
            temrct.points.append(CMvPoint(temdata.x - 0.5 * CropSize.width(), temdata.y - 0.5 * CropSize.width()));
            temrct.points.append(CMvPoint(temdata.x + 0.5 * CropSize.width(), temdata.y - 0.5 * CropSize.width()));
            temrct.points.append(CMvPoint(temdata.x + 0.5 * CropSize.width(), temdata.y + 0.5 * CropSize.width()));
            temrct.points.append(CMvPoint(temdata.x - 0.5 * CropSize.width(), temdata.y + 0.5 * CropSize.width()));
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(
                LabTxt(temp.Name, CMvPoint(temdata.x, temdata.y), QFont("Times", 13, 14), Qt::white, 0, Qt::blue));
            break;
        }
        case LabelSet::LpolygonROI:
        {
            CMvPolygon temdata;
            temdata.GetData(temp.toGetRoiData());

            CMvRotatedRect temrct = toGetPRota(temdata.points);
            setshow.append(LabTxt(temp.Name,
                                  CMvPoint(temrct.Center.x, temrct.Center.y),
                                  QFont("Times", 13, 14),
                                  Qt::white,
                                  0,
                                  Qt::blue));
            setshow.append(DispRotatedRects(temrct, ColorLis.at(GetIndex), 1));

            break;
        }
        case LabelSet::LcircleROI:
        {
            CMvCircle temdata;
            temdata.GetData(temp.toGetRoiData());

            CMvPolygon temrct;
            temrct.points.append(CMvPoint(temdata.center.x - temdata.radius, temdata.center.y - temdata.radius));
            temrct.points.append(CMvPoint(temdata.center.x + temdata.radius, temdata.center.y - temdata.radius));
            temrct.points.append(CMvPoint(temdata.center.x + temdata.radius, temdata.center.y + temdata.radius));
            temrct.points.append(CMvPoint(temdata.center.x - temdata.radius, temdata.center.y + temdata.radius));
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(LabTxt(temp.Name,
                                  CMvPoint(temdata.center.x, temdata.center.y),
                                  QFont("Times", 13, 14),
                                  Qt::white,
                                  0,
                                  Qt::blue));

            break;
        }
        }
    }
}

void YtRoiLabelSet::toLoadObbTxt(QString filename, QSize imsize, QVector<QString> NameList)
{
    qDebug() << "toLoadDetcTxt";
    int getindex = filename.lastIndexOf(".");
    if (getindex < 0)
    {
        return;
    }
    toClearData();
    QFile file(QString("%1.txt").arg(filename.left(getindex)));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }
    //
    toSetImSize(imsize);
    QFileInfo temi(filename);
    toSetFileName(temi.fileName());

    QTextStream in(&file);
    QStringList Getlist;
    LabelSet temp;
    CMvPolygon temdata;
    while (!in.atEnd())
    {
        QString line = in.readLine();
        // 处理每行文本
        Getlist = line.split(" ");
        if (Getlist.size() < 5)
        {
            continue;
        }
        int getNameindex = QString(Getlist[0]).toInt();
        if (getNameindex >= NameList.size())
        {
            continue;
        }
        // 说白了 就是4个点转换成斜矩形

        //
        temdata.toClearData();
        if (Getlist.size() % 2 == 1)
        {
            continue;
        }
        for (int j = 1; j < Getlist.size() - 1; j += 2)
        {
            temdata.points.append(CMvPoint(QString(Getlist[j]).toDouble() * imsize.width(),
                                           QString(Getlist[j + 1]).toDouble() * imsize.height()));
        }
        //
        CMvRotatedRect touy = toGetPRota(temdata.points);
        if (NameList.size() > getNameindex && getNameindex >= 0)
        {
            temp.toInitProData(NameList.at(getNameindex), LabelSet::LrotaterectangleROI, touy.Data());
            toAppendData(temp);
        }

        //        temp.toInitProData(NameList.at(getNameindex),LabelSet::LpolygonROI,temdata.Data());
        //        toAppendData(temp);
    }

    file.close();
}

void YtRoiLabelSet::toSaveSegTxt(QString Path, QVector<QString> NameList, QSize CropSize)
{

    // 保存就要给路径
    int getindex = m_imagePath.lastIndexOf(".");
    if (getindex < 0)
    {
        return;
    }

    QFile file(QString("%1/%2.txt").arg(Path).arg(m_imagePath.left(getindex)));
    qDebug() << file.fileName() << "YtRoiLabelSet::toSaveSegTxt";
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }

    QTextStream s(&file);
    QString temstr;
    // 创建一个JSON对象
    for (int i = 0; i < m_SetLabeset.size(); i++)
    {
        // 在这里逐行数据书写
        LabelSet temp = m_SetLabeset.at(i);
        int GetIndex = NameList.indexOf(temp.Name);
        qDebug() << GetIndex << "YtRoiLabelSet::toSaveSegTxt PP" << i << NameList << temp.Name;
        if (GetIndex < 0)
        {
            continue;
        }
        switch (temp.toGetRoiType())
        {
        case LabelSet::LrectangleROI:
        {
            CMvRect temdata;
            temdata.GetData(temp.toGetRoiData());
            // 获取四个点
            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       temdata.LeftTop.x / m_imWidth,
                                       temdata.LeftTop.y / m_imHight,
                                       (temdata.LeftTop.x + temdata.cx) / m_imWidth,
                                       (temdata.LeftTop.y) / m_imHight,
                                       (temdata.LeftTop.x + temdata.cx) / m_imWidth,
                                       (temdata.LeftTop.y + temdata.cy) / m_imHight,
                                       (temdata.LeftTop.x) / m_imWidth,
                                       (temdata.LeftTop.y + temdata.cy) / m_imHight);
            s << temstr << "\n";

            break;
        }
        case LabelSet::LrotaterectangleROI:
        {
            CMvRotatedRect temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            QVector<CMvPoint> Setpoints;
            CMvPoint getpos[4];
            temdata.toGetPoint(getpos);
            Setpoints << getpos[0] << getpos[1] << getpos[2] << getpos[3];
            //
            // CMvRect temrct=toGetPointS(Setpoints);
            //
            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       getpos[0].x / m_imWidth,
                                       getpos[0].y / m_imHight,
                                       getpos[1].x / m_imWidth,
                                       getpos[1].y / m_imHight,
                                       getpos[2].x / m_imWidth,
                                       getpos[2].y / m_imHight,
                                       getpos[3].x / m_imWidth,
                                       getpos[3].y / m_imHight);
            s << temstr << "\n";
            break;
        }
        case LabelSet::LpointROI:
        {
            CMvPoint temdata;
            temdata.GetData(temp.toGetRoiData());

            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       (temdata.x - 0.5 * CropSize.width()) / m_imWidth,
                                       (temdata.y - 0.5 * CropSize.height()) / m_imHight,
                                       (temdata.x + 0.5 * CropSize.width()) / m_imWidth,
                                       (temdata.y - 0.5 * CropSize.height()) / m_imHight,
                                       (temdata.x + 0.5 * CropSize.width()) / m_imWidth,
                                       (temdata.y + 0.5 * CropSize.height()) / m_imHight,
                                       (temdata.x - 0.5 * CropSize.width()) / m_imWidth,
                                       (temdata.y + 0.5 * CropSize.height()) / m_imHight);
            s << temstr << "\n";
            break;
        }
        case LabelSet::LpolygonROI:
        {
            CMvPolygon temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            // CMvRect temrct=toGetPointS(temdata.points);
            //
            for (int i = 0; i < temdata.points.size(); i++)
            {
                if (i == 0)
                {
                    temstr = QString::asprintf("%d %1.6f %1.6f",
                                               GetIndex,
                                               temdata.points[i].x / m_imWidth,
                                               temdata.points[i].y / m_imWidth);
                }
                else
                {
                    temstr.append(QString(" %1 %2")
                                      .arg(QString::number(temdata.points[i].x / m_imWidth, 'f', 6))
                                      .arg(QString::number(temdata.points[i].y / m_imWidth, 'f', 6)));
                }
            }

            s << temstr << "\n";

            break;
        }
        case LabelSet::LcircleROI:
        {
            CMvCircle temdata;
            temdata.GetData(temp.toGetRoiData());

            temstr = QString::asprintf("%d %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f %1.6f",
                                       GetIndex,
                                       (temdata.center.x - temdata.radius) / m_imWidth,
                                       (temdata.center.y - temdata.radius) / m_imHight,
                                       (temdata.center.x + temdata.radius) / m_imWidth,
                                       (temdata.center.y - temdata.radius) / m_imHight,
                                       (temdata.center.x + temdata.radius) / m_imWidth,
                                       (temdata.center.y + temdata.radius) / m_imHight,
                                       (temdata.center.x - temdata.radius) / m_imWidth,
                                       (temdata.center.y + temdata.radius) / m_imHight);
            s << temstr << "\n";

            break;
        }
        }
    }
    file.close();
}

void YtRoiLabelSet::toGenSegOveplay(YtSetShowtObj &setshow,
                                    QVector<QString> NameList,
                                    QVector<QColor> ColorLis,
                                    QSize CropSize)
{

    setshow.toClearData();
    // 创建一个JSON对象
    for (int i = 0; i < m_SetLabeset.size(); i++)
    {
        // 在这里逐行数据书写
        LabelSet temp = m_SetLabeset.at(i);
        int GetIndex = NameList.indexOf(temp.Name);
        if (GetIndex < 0)
        {
            continue;
        }
        switch (temp.toGetRoiType())
        {
        case LabelSet::LrectangleROI:
        {
            CMvRect temdata;
            temdata.GetData(temp.toGetRoiData());
            // 获取四个点
            CMvPolygon temrct;
            temrct.points.append(CMvPoint(temdata.LeftTop.x, temdata.LeftTop.y));
            temrct.points.append(CMvPoint(temdata.LeftTop.x + temdata.cx, temdata.LeftTop.y));
            temrct.points.append(CMvPoint(temdata.LeftTop.x + temdata.cx, temdata.LeftTop.y + temdata.cy));
            temrct.points.append(CMvPoint(temdata.LeftTop.x, temdata.LeftTop.y + temdata.cy));
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(LabTxt(temp.Name,
                                  CMvPoint(temdata.LeftTop.x + temdata.cx / 2, temdata.LeftTop.y + temdata.cy / 2),
                                  QFont("Times", 13, 14),
                                  Qt::white,
                                  0,
                                  Qt::blue));

            break;
        }
        case LabelSet::LrotaterectangleROI:
        {
            CMvRotatedRect temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            QVector<CMvPoint> Setpoints;
            CMvPoint getpos[4];
            temdata.toGetPoint(getpos);
            Setpoints << getpos[0] << getpos[1] << getpos[2] << getpos[3];
            //
            //
            CMvPolygon temrct;
            temrct.points.append(getpos[0]);
            temrct.points.append(getpos[1]);
            temrct.points.append(getpos[2]);
            temrct.points.append(getpos[3]);
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(LabTxt(temp.Name,
                                  CMvPoint(temdata.Center.x, temdata.Center.y),
                                  QFont("Times", 13, 14),
                                  Qt::white,
                                  0,
                                  Qt::blue));

            break;
        }
        case LabelSet::LpointROI:
        {
            CMvPoint temdata;
            temdata.GetData(temp.toGetRoiData());

            CMvPolygon temrct;
            temrct.points.append(CMvPoint(temdata.x - 0.5 * CropSize.width(), temdata.y - 0.5 * CropSize.width()));
            temrct.points.append(CMvPoint(temdata.x + 0.5 * CropSize.width(), temdata.y - 0.5 * CropSize.width()));
            temrct.points.append(CMvPoint(temdata.x + 0.5 * CropSize.width(), temdata.y + 0.5 * CropSize.width()));
            temrct.points.append(CMvPoint(temdata.x - 0.5 * CropSize.width(), temdata.y + 0.5 * CropSize.width()));
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(
                LabTxt(temp.Name, CMvPoint(temdata.x, temdata.y), QFont("Times", 13, 14), Qt::white, 0, Qt::blue));
            break;
        }
        case LabelSet::LpolygonROI:
        {
            CMvPolygon temdata;
            temdata.GetData(temp.toGetRoiData());

            CMvPolygon temrct;

            for (int i = 0; i < temdata.points.size(); i++)
            {
                if (i == 0)
                {
                    setshow.append(LabTxt(temp.Name,
                                          CMvPoint(temdata.points.at(0).x, temdata.points.at(0).y),
                                          QFont("Times", 13, 14),
                                          Qt::white,
                                          0,
                                          Qt::blue));
                }
                temrct.points.append(temdata.points.at(i));
            }
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));

            break;
        }
        case LabelSet::LcircleROI:
        {
            CMvCircle temdata;
            temdata.GetData(temp.toGetRoiData());

            CMvPolygon temrct;
            temrct.points.append(CMvPoint(temdata.center.x - temdata.radius, temdata.center.y - temdata.radius));
            temrct.points.append(CMvPoint(temdata.center.x + temdata.radius, temdata.center.y - temdata.radius));
            temrct.points.append(CMvPoint(temdata.center.x + temdata.radius, temdata.center.y + temdata.radius));
            temrct.points.append(CMvPoint(temdata.center.x - temdata.radius, temdata.center.y + temdata.radius));
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(LabTxt(temp.Name,
                                  CMvPoint(temdata.center.x, temdata.center.y),
                                  QFont("Times", 13, 14),
                                  Qt::white,
                                  0,
                                  Qt::blue));

            break;
        }
        }
    }
}

void YtRoiLabelSet::toLoadSegTxt(QString filename, QSize imsize, QVector<QString> NameList)
{
    qDebug() << "toLoadDetcTxt";
    int getindex = filename.lastIndexOf(".");
    if (getindex < 0)
    {
        return;
    }
    toClearData();
    QFile file(QString("%1.txt").arg(filename.left(getindex)));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }
    //
    toSetImSize(imsize);
    QFileInfo temi(filename);
    toSetFileName(temi.fileName());

    QTextStream in(&file);
    QStringList Getlist;
    LabelSet temp;
    CMvPolygon temdata;
    while (!in.atEnd())
    {
        QString line = in.readLine();
        // 处理每行文本
        Getlist = line.split(" ", Qt::SkipEmptyParts);
        if (Getlist.size() < 7)
        {
            qWarning() << "errorMessage: invalid YOLO segmentation row" << line;
            continue;
        }
        bool isClassIndexValid = false;
        int getNameindex = Getlist.at(0).toInt(&isClassIndexValid);
        if (!isClassIndexValid || getNameindex < 0 || getNameindex >= NameList.size())
        {
            qWarning() << "errorMessage: invalid YOLO segmentation class index" << Getlist.at(0);
            continue;
        }
        int coordinateEnd = Getlist.size();
        if (coordinateEnd % 2 == 0)
        {
            coordinateEnd--;
        }
        temdata.toClearData();
        for (int j = 1; j + 1 < coordinateEnd; j += 2)
        {
            temdata.points.append(
                CMvPoint(Getlist.at(j).toDouble() * imsize.width(), Getlist.at(j + 1).toDouble() * imsize.height()));
        }
        temp.toInitProData(NameList.at(getNameindex), LabelSet::LpolygonROI, temdata.Data());
        toAppendData(temp);
    }

    file.close();
}

void YtRoiLabelSet::toSaveClassTxt(QString Path, QVector<QString> NameList, QSize CropSize)
{
}

void YtRoiLabelSet::toGenClassOveplay(YtSetShowtObj &setshow,
                                      QVector<QString> NameList,
                                      QVector<QColor> ColorLis,
                                      QSize CropSize)
{
    setshow.toClearData();
    // 创建一个JSON对象
    for (int i = 0; i < m_SetLabeset.size(); i++)
    {
        // 在这里逐行数据书写
        LabelSet temp = m_SetLabeset.at(i);
        int GetIndex = NameList.indexOf(temp.Name);
        if (GetIndex < 0)
        {
            continue;
        }
        switch (temp.toGetRoiType())
        {
        case LabelSet::LrectangleROI:
        {
            CMvRect temdata;
            temdata.GetData(temp.toGetRoiData());
            // 获取四个点
            setshow.append(DispRects(temdata, ColorLis.at(GetIndex), 1));
            setshow.append(DispTxt(temp.Name,
                                   CMvPoint(temdata.LeftTop.x, temdata.LeftTop.y - 13),
                                   QFont("Times", 13, 14),
                                   Qt::blue));

            break;
        }
        case LabelSet::LrotaterectangleROI:
        {
            CMvRotatedRect temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            QVector<CMvPoint> Setpoints;
            CMvPoint getpos[4];
            temdata.toGetPoint(getpos);
            Setpoints << getpos[0] << getpos[1] << getpos[2] << getpos[3];
            //
            CMvRect temrct = toGetPointS(Setpoints);
            setshow.append(DispRects(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(DispTxt(temp.Name,
                                   CMvPoint(temrct.LeftTop.x, temrct.LeftTop.y - 13),
                                   QFont("Times", 13, 14),
                                   Qt::blue));

            break;
        }
        case LabelSet::LpointROI:
        {
            CMvPoint temdata;
            temdata.GetData(temp.toGetRoiData());
            CMvRect temrct = CMvRect(temdata.x - CropSize.width() / 2,
                                     temdata.y - CropSize.height() / 2,
                                     CropSize.width(),
                                     CropSize.height());
            setshow.append(DispRects(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(DispTxt(temp.Name,
                                   CMvPoint(temrct.LeftTop.x, temrct.LeftTop.y - 13),
                                   QFont("Times", 13, 14),
                                   Qt::blue));
            break;
        }
        case LabelSet::LpolygonROI:
        {
            CMvPolygon temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            CMvRect temrct = toGetPointS(temdata.points);
            setshow.append(DispRects(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(DispTxt(temp.Name,
                                   CMvPoint(temrct.LeftTop.x, temrct.LeftTop.y - 13),
                                   QFont("Times", 13, 14),
                                   Qt::blue));
            break;
        }
        case LabelSet::LcircleROI:
        {
            CMvCircle temdata;
            temdata.GetData(temp.toGetRoiData());
            CMvRect temrct = CMvRect(temdata.center.x - temdata.radius,
                                     temdata.center.y - temdata.radius,
                                     2 * temdata.radius,
                                     2 * temdata.radius);
            setshow.append(DispRects(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(DispTxt(temp.Name,
                                   CMvPoint(temrct.LeftTop.x, temrct.LeftTop.y - 13),
                                   QFont("Times", 13, 14),
                                   Qt::blue));
            break;
        }
        }
    }
}

void YtRoiLabelSet::toLoadClassTxt(QString filename, QSize imsize, QVector<QString> NameList)
{
    qDebug() << "toLoadDetcTxt";

    int getindex = filename.lastIndexOf(".");
    if (getindex < 0)
    {
        return;
    }
    toClearData();
    QFile file(QString("%1.txt").arg(filename.left(getindex)));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }
    //
    toSetImSize(imsize);
    QFileInfo temi(filename);
    toSetFileName(temi.fileName());

    QTextStream in(&file);
    QStringList Getlist;
    LabelSet temp;
    CMvRect temdata;
    int indexcoun = 0;
    while (!in.atEnd())
    {
        QString line = in.readLine();

        // 处理每行文本
        Getlist = line.split(" ");
        if (Getlist.size() < 2)
        {
            continue;
        }
        indexcoun += 1;
        //        int getNameindex=QString(Getlist[1]).toInt();
        //        if(getNameindex>=NameList.size())
        //        {
        //            continue;
        //        }
        double Width = imsize.width();
        double Height = imsize.height();
        //
        temdata.LeftTop.x = 0;
        temdata.LeftTop.y = 0;
        temdata.cx = Width - 1;
        temdata.cy = Height - 1;
        //
        //        temp.toInitProData(NameList.at(getNameindex),LabelSet::LrectangleROI,temdata.Data());
        temp.toInitProData(Getlist[1], LabelSet::LrectangleROI, temdata.Data());
        toAppendData(temp);

        if (indexcoun == 1)
        {
            break;
        }
    }

    file.close();
}

QString YtRoiLabelSet::toLoadOcrTxt(QString filename, QSize imsize, QVector<QString> NameList)
{
    int getindex = filename.lastIndexOf(".");
    if (getindex < 0)
    {
        return "";
    }
    toClearData();
    QFile file(QString("%1.txt").arg(filename.left(getindex)));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return "";
    }
    //
    toSetImSize(imsize);
    QFileInfo temi(filename);
    toSetFileName(temi.fileName());

    QTextStream in(&file);
    QStringList Getlist;
    LabelSet temp;
    CMvRect temdata;
    int indexcoun = 0;
    QString getstr = "";
    while (!in.atEnd())
    {
        QString line = in.readLine();
        getstr = line;
    }
    temdata.LeftTop.x = 0;
    temdata.LeftTop.y = 0;
    temdata.cx = imsize.width();
    temdata.cy = imsize.height();
    temp.toInitProData(getstr, LabelSet::LrectangleROI, temdata.Data());
    toAppendData(temp);
    file.close();
    return getstr;
}

void YtRoiLabelSet::toSaveFourPosTxt(QString Path, QVector<QString> NameList, QSize CropSize)
{
    // 保存就要给路径
    int getindex = m_imagePath.lastIndexOf(".");
    if (getindex < 0)
    {
        return;
    }

    QFile file(QString("%1/%2.txt").arg(Path).arg(m_imagePath.left(getindex)));
    qDebug() << file.fileName() << "YtRoiLabelSet::toSaveSegTxt";
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }

    QTextStream s(&file);
    QString temstr;
    // 创建一个JSON对象
    for (int i = 0; i < m_SetLabeset.size(); i++)
    {
        // 在这里逐行数据书写
        LabelSet temp = m_SetLabeset.at(i);
        int GetIndex = 0;
        qDebug() << GetIndex << "YtRoiLabelSet::toSaveSegTxt PP" << i << NameList << temp.Name;

        switch (temp.toGetRoiType())
        {
        case LabelSet::LrectangleROI:
        {
            CMvRect temdata;
            temdata.GetData(temp.toGetRoiData());
            // 获取四个点
            temstr = QString::asprintf("%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%d",
                                       temdata.LeftTop.x,
                                       temdata.LeftTop.y,
                                       (temdata.LeftTop.x + temdata.cx),
                                       (temdata.LeftTop.y),
                                       (temdata.LeftTop.x + temdata.cx),
                                       (temdata.LeftTop.y + temdata.cy),
                                       (temdata.LeftTop.x),
                                       (temdata.LeftTop.y + temdata.cy),
                                       GetIndex);
            s << temstr << "\n";

            break;
        }
        case LabelSet::LrotaterectangleROI:
        {
            CMvRotatedRect temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            QVector<CMvPoint> Setpoints;
            CMvPoint getpos[4];
            temdata.toGetPoint(getpos);
            Setpoints << getpos[0] << getpos[1] << getpos[2] << getpos[3];
            //
            // CMvRect temrct=toGetPointS(Setpoints);
            //
            temstr = QString::asprintf("%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%d",
                                       GetIndex,
                                       getpos[0].x,
                                       getpos[0].y,
                                       getpos[1].x,
                                       getpos[1].y,
                                       getpos[2].x,
                                       getpos[2].y,
                                       getpos[3].x,
                                       getpos[3].y);
            s << temstr << "\n";
            break;
        }
        case LabelSet::LpointROI:
        {
            CMvPoint temdata;
            temdata.GetData(temp.toGetRoiData());

            temstr = QString::asprintf("%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%d",
                                       GetIndex,
                                       (temdata.x - 0.5 * CropSize.width()),
                                       (temdata.y - 0.5 * CropSize.height()),
                                       (temdata.x + 0.5 * CropSize.width()),
                                       (temdata.y - 0.5 * CropSize.height()),
                                       (temdata.x + 0.5 * CropSize.width()),
                                       (temdata.y + 0.5 * CropSize.height()),
                                       (temdata.x - 0.5 * CropSize.width()),
                                       (temdata.y + 0.5 * CropSize.height()));
            s << temstr << "\n";
            break;
        }
        case LabelSet::LpolygonROI:
        {
            CMvPolygon temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            // CMvRect temrct=toGetPointS(temdata.points);
            //
            for (int j = 0; j < temdata.points.size(); j++)
            {
                if (j == temdata.points.size() - 1)
                {
                    QString ptemstr;
                    ptemstr = QString::asprintf("%1.0f,%1.0f,%d", temdata.points[j].x, temdata.points[j].y, GetIndex);
                    temstr.append(ptemstr);
                }
                else
                {
                    temstr.append(QString("%1,%2,")
                                      .arg(QString::number(temdata.points[j].x, 'f', 0))
                                      .arg(QString::number(temdata.points[j].y, 'f', 0)));
                }
            }

            s << temstr << "\n";

            break;
        }
        case LabelSet::LcircleROI:
        {
            CMvCircle temdata;
            temdata.GetData(temp.toGetRoiData());

            temstr = QString::asprintf("%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%1.0f,%d",
                                       (temdata.center.x - temdata.radius),
                                       (temdata.center.y - temdata.radius),
                                       (temdata.center.x + temdata.radius),
                                       (temdata.center.y - temdata.radius),
                                       (temdata.center.x + temdata.radius),
                                       (temdata.center.y + temdata.radius),
                                       (temdata.center.x - temdata.radius),
                                       (temdata.center.y + temdata.radius),
                                       GetIndex);
            s << temstr << "\n";

            break;
        }
        }
    }
    file.close();
}

void YtRoiLabelSet::toGenFourPosOveplay(YtSetShowtObj &setshow,
                                        QVector<QString> NameList,
                                        QVector<QColor> ColorLis,
                                        QSize CropSize)
{
    setshow.toClearData();
    // 创建一个JSON对象
    for (int i = 0; i < m_SetLabeset.size(); i++)
    {
        // 在这里逐行数据书写
        LabelSet temp = m_SetLabeset.at(i);
        int GetIndex = NameList.indexOf(temp.Name);
        if (GetIndex < 0)
        {
            continue;
        }
        switch (temp.toGetRoiType())
        {
        case LabelSet::LrectangleROI:
        {
            CMvRect temdata;
            temdata.GetData(temp.toGetRoiData());
            // 获取四个点
            CMvPolygon temrct;
            temrct.points.append(CMvPoint(temdata.LeftTop.x, temdata.LeftTop.y));
            temrct.points.append(CMvPoint(temdata.LeftTop.x + temdata.cx, temdata.LeftTop.y));
            temrct.points.append(CMvPoint(temdata.LeftTop.x + temdata.cx, temdata.LeftTop.y + temdata.cy));
            temrct.points.append(CMvPoint(temdata.LeftTop.x, temdata.LeftTop.y + temdata.cy));
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(LabTxt(temp.Name,
                                  CMvPoint(temdata.LeftTop.x + temdata.cx / 2, temdata.LeftTop.y + temdata.cy / 2),
                                  QFont("Times", 13, 14),
                                  Qt::white,
                                  0,
                                  Qt::blue));

            break;
        }
        case LabelSet::LrotaterectangleROI:
        {
            CMvRotatedRect temdata;
            temdata.GetData(temp.toGetRoiData());
            //
            QVector<CMvPoint> Setpoints;
            CMvPoint getpos[4];
            temdata.toGetPoint(getpos);
            Setpoints << getpos[0] << getpos[1] << getpos[2] << getpos[3];
            //
            //
            CMvPolygon temrct;
            temrct.points.append(getpos[0]);
            temrct.points.append(getpos[1]);
            temrct.points.append(getpos[2]);
            temrct.points.append(getpos[3]);
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(LabTxt(temp.Name,
                                  CMvPoint(temdata.Center.x, temdata.Center.y),
                                  QFont("Times", 13, 14),
                                  Qt::white,
                                  0,
                                  Qt::blue));

            break;
        }
        case LabelSet::LpointROI:
        {
            CMvPoint temdata;
            temdata.GetData(temp.toGetRoiData());

            CMvPolygon temrct;
            temrct.points.append(CMvPoint(temdata.x - 0.5 * CropSize.width(), temdata.y - 0.5 * CropSize.width()));
            temrct.points.append(CMvPoint(temdata.x + 0.5 * CropSize.width(), temdata.y - 0.5 * CropSize.width()));
            temrct.points.append(CMvPoint(temdata.x + 0.5 * CropSize.width(), temdata.y + 0.5 * CropSize.width()));
            temrct.points.append(CMvPoint(temdata.x - 0.5 * CropSize.width(), temdata.y + 0.5 * CropSize.width()));
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(
                LabTxt(temp.Name, CMvPoint(temdata.x, temdata.y), QFont("Times", 13, 14), Qt::white, 0, Qt::blue));
            break;
        }
        case LabelSet::LpolygonROI:
        {
            CMvPolygon temdata;
            temdata.GetData(temp.toGetRoiData());

            CMvPolygon temrct;

            for (int i = 0; i < temdata.points.size(); i++)
            {
                if (i == 0)
                {
                    setshow.append(LabTxt(temp.Name,
                                          CMvPoint(temdata.points.at(0).x, temdata.points.at(0).y),
                                          QFont("Times", 13, 14),
                                          Qt::white,
                                          0,
                                          Qt::blue));
                }
                temrct.points.append(temdata.points.at(i));
            }
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));

            break;
        }
        case LabelSet::LcircleROI:
        {
            CMvCircle temdata;
            temdata.GetData(temp.toGetRoiData());

            CMvPolygon temrct;
            temrct.points.append(CMvPoint(temdata.center.x - temdata.radius, temdata.center.y - temdata.radius));
            temrct.points.append(CMvPoint(temdata.center.x + temdata.radius, temdata.center.y - temdata.radius));
            temrct.points.append(CMvPoint(temdata.center.x + temdata.radius, temdata.center.y + temdata.radius));
            temrct.points.append(CMvPoint(temdata.center.x - temdata.radius, temdata.center.y + temdata.radius));
            setshow.append(DispPolygons(temrct, ColorLis.at(GetIndex), 1));
            setshow.append(LabTxt(temp.Name,
                                  CMvPoint(temdata.center.x, temdata.center.y),
                                  QFont("Times", 13, 14),
                                  Qt::white,
                                  0,
                                  Qt::blue));

            break;
        }
        }
    }
}

void YtRoiLabelSet::toLoadFourPosTxt(QString filename, QSize imsize, QVector<QString> NameList)
{
    qDebug() << "toLoadFourPosTxt";
    int getindex = filename.lastIndexOf(".");
    if (getindex < 0)
    {
        return;
    }
    toClearData();
    QFile file(QString("%1.txt").arg(filename.left(getindex)));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }
    //
    toSetImSize(imsize);
    QFileInfo temi(filename);
    toSetFileName(temi.fileName());

    QTextStream in(&file);
    QStringList Getlist;
    LabelSet temp;
    CMvPolygon temdata;
    while (!in.atEnd())
    {
        QString line = in.readLine();
        // 处理每行文本
        Getlist = line.split(",");
        if (Getlist.size() < 9)
        {
            continue;
        }
        int getNameindex = QString(Getlist.last()).toInt();
        if (getNameindex >= NameList.size())
        {
            continue;
        }
        //
        temdata.toClearData();
        for (int j = 0; j < Getlist.size() - 1; j += 2)
        {
            temdata.points.append(CMvPoint(QString(Getlist[j]).toDouble(), QString(Getlist[j + 1]).toDouble()));
        }
        //
        temp.toInitProData(NameList.at(getNameindex), LabelSet::LpolygonROI, temdata.Data());
        toAppendData(temp);
    }

    file.close();
}

void YtRoiLabelSet::toSetImSize(QSize ImSize)
{
    m_imHight = ImSize.height();
    m_imWidth = ImSize.width();
}

QSize YtRoiLabelSet::toGetImSize()
{
    return QSize(m_imWidth, m_imHight);
}

void YtRoiLabelSet::toSetFileName(QString filename)
{
    m_imagePath = filename;
}

QString YtRoiLabelSet::toGetFileName()
{
    return m_imagePath;
}

static QString m_WorkPath;   // 也就是非空从文件走
static QString m_PythonPath; //

QString YtYoloDefine::toGetWorkPath()
{
    if (m_WorkPath.isEmpty())
    {
        QFile file(QString("%1/%2.json").arg(QDir::currentPath()).arg("YtYoloDefine"));
        if (file.open(QIODevice::ReadOnly))
        {
            const QJsonDocument jsonDocument = QJsonDocument::fromJson(file.readAll());
            file.close();
            m_WorkPath = jsonDocument.object().value(QStringLiteral("WorkPath")).toString();
        }
    }

    if (m_WorkPath.isEmpty() || !QDir(m_WorkPath).exists())
    {
        const QString workPath = QFileDialog::getExistingDirectory(nullptr,
                                                                   QString(u8"选择工作目录"),
                                                                   QDir::homePath(),
                                                                   QFileDialog::ShowDirsOnly);
        if (workPath.isEmpty())
        {
            qCritical().noquote() << QString(u8"未选择工作目录");
            return QString();
        }
        toSetWorkPath(workPath);
    }

    return m_WorkPath;
}

void YtYoloDefine::toSetWorkPath(QString WorkPath)
{
    if (m_WorkPath != WorkPath)
    {
        m_WorkPath = WorkPath;
    }
    QFile file(QString("%1/%2.json").arg(QDir::currentPath()).arg("YtYoloDefine"));
    if (!file.open(QIODevice::WriteOnly))
    {
        qCritical().noquote() << QString(u8"无法保存工作目录配置: %1").arg(file.fileName());
        return;
    }
    QJsonObject jsonObject;
    jsonObject.insert("WorkPath", m_WorkPath);
    QJsonDocument jsonDoc(jsonObject);
    file.write(jsonDoc.toJson(QJsonDocument::Indented));
    file.close();
}

QString YtYoloDefine::toGetPythonPath()
{
    if (m_PythonPath.isEmpty())
    {
        // 尝试从本地文件读取
        QFile file(QString("%1/%2.json").arg(QDir::currentPath()).arg("YtYoloDefine"));
        if (file.open(QIODevice::ReadOnly))
        {
            // 文件打开成功
            QByteArray jsonData = file.readAll();
            file.close();
            // 从QByteArray解析JSON文档
            QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);

            // 确保JSON文档是一个对象
            if (jsonDoc.isObject())
            {
                QJsonObject jsonObject = jsonDoc.object();
                m_PythonPath = jsonObject["PythonPath"].toString();
            }
        }
    }
    return m_PythonPath;
}

void YtYoloDefine::toSetPythonPath(QString PythPath)
{
    if (m_PythonPath != PythPath)
    {
        m_PythonPath = PythPath;
    }
    // 强制保存
    QFile file(QString("%1/%2.json").arg(QDir::currentPath()).arg("YtYoloDefine"));
    if (!file.open(QIODevice::WriteOnly))
    {

        return;
    }
    // 创建一个JSON对象
    QJsonObject jsonObject;

    jsonObject.insert("WorkPath", m_WorkPath);
    jsonObject.insert("PythonPath", m_PythonPath);
    // 创建一个JSON文档
    QJsonDocument jsonDoc(jsonObject);
    // 写入到文件
    file.write(jsonDoc.toJson(QJsonDocument::Indented));
    file.close();
}

QString YtYoloDefine::toGetLabelPath()
{
    return QString("%1/%2").arg(m_WorkPath).arg(u8"LabelSheet");
}

QString YtYoloDefine::toGetDataPath()
{
    return QString("%1/%2").arg(m_WorkPath).arg(u8"DataSheet");
}

QString YtYoloDefine::toGetTrainPath()
{
    return QString("%1/%2").arg(m_WorkPath).arg(u8"TrainSheet");
}

QString YtYoloDefine::toGetValuePath()
{
    return QString("%1/%2").arg(m_WorkPath).arg(u8"ValSheet");
}

QFileInfoList YtYoloDefine::toGetPathDirInfo(QString Path)
{
    QDir temdir;
    temdir.setPath(Path);
    QFileInfoList temlis = temdir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    return temlis;
}

QFileInfoList YtYoloDefine::toGetPathFileInfo(QString Path, QStringList FilterName)
{
    QDir temdir;
    temdir.setPath(Path);
    QFileInfoList temlis = temdir.entryInfoList(FilterName, QDir::Files, QDir::Time);
    return temlis;
}

QFileInfoList
YtYoloDefine::toGetPathSpecialFileInfo(QString Path, QStringList FilterName1, QString FilterName2, bool isexit)
{
    QDir temdir;
    temdir.setPath(Path);
    QFileInfoList temlis = temdir.entryInfoList(FilterName1, QDir::Files, QDir::Time);

    QFileInfoList outlis;

    //
    for (int i = 0; i < temlis.size(); i++)
    {
        QString filename = temlis.at(i).absoluteFilePath();
        int getindex = filename.lastIndexOf(".");
        if (getindex < 0)
        {
            continue;
        }
        if (isexit)
        {
            if (QFile::exists(filename.left(getindex) + "." + FilterName2))
            {
                outlis.append(temlis.at(i));
            }
        }
        else
        {
            if (!QFile::exists(filename.left(getindex) + "." + FilterName2))
            {
                outlis.append(temlis.at(i));
            }
        }
    }
    return outlis;
}

void YtYoloDefine::toSaveSetting(QString WorkingPath)
{
    // 保存就要给路径

    QFile file(QString("%1/%2.json")
                   .arg(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation))
                   .arg("AppData/Roaming/Ultralytics/settings"));
    if (!file.exists())
    {
        return;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }
    QStringList temlis;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    while (!out.atEnd())
    {
        temlis.append(out.readLine());
    }
    file.close();
    qDebug() << file.fileName() << temlis;
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }
    qDebug() << temlis << "ZZ" << WorkingPath;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    //
    for (int i = 0; i < temlis.size(); i++)
    {
        if (temlis.at(i).contains("wandb"))
        {
            in << QString("  \"wandb\": false,") << Qt::endl;
        }
        else if (temlis.at(i).contains("mlflow"))
        {
            in << QString("  \"mlflow\": false,") << Qt::endl;
        }
        else if (temlis.at(i).contains("runs_dir"))
        {
            in << QString("  \"runs_dir\": \"%1\",").arg(QString(WorkingPath.replace("/", "\\\\"))) << Qt::endl;
        }
        else
        {
            in << temlis.at(i) << Qt::endl;
        }
    }

    file.close();
}

void YtYoloDefine::toProFontData()
{
    const QString fontDirectory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
        + "/AppData/Roaming/Ultralytics";
    const QDir directory(fontDirectory);
    if (!directory.exists() && !QDir().mkpath(fontDirectory))
    {
        qCritical() << "创建 Ultralytics 字体目录失败:" << fontDirectory;
        return;
    }

    const QStringList fontNames = { "Arial.ttf", "Arial.Unicode.ttf" };
    for (const QString &fontName : fontNames)
    {
        const QString destinationPath = directory.filePath(fontName);
        if (QFile::exists(destinationPath))
        {
            continue;
        }

        const QString sourcePath = QDir(QCoreApplication::applicationDirPath()).filePath(fontName);
        if (!QFile::exists(sourcePath))
        {
            qCritical() << "缺少 Ultralytics 字体文件:" << sourcePath;
            continue;
        }

        if (!QFile::copy(sourcePath, destinationPath))
        {
            qCritical() << "复制 Ultralytics 字体文件失败:" << sourcePath << destinationPath;
        }
    }
}

YtYoloSetPro::YtYoloSetPro()
{
}

void YtYoloSetPro::toLoadData(QString Path)
{
    m_InfoSet.clear();
    m_ClorDefine.clear();
    m_NameList.clear();
    m_UncheckedDataSheetList.clear();
    //
    // 尝试从本地文件读取
    QFile file(QString("%1/%2.json").arg(Path).arg("DefineLabel"));
    if (file.open(QIODevice::ReadOnly))
    {
        // 文件打开成功
        QByteArray jsonData = file.readAll();
        file.close();
        // 从QByteArray解析JSON文档
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);

        // 确保JSON文档是一个对象
        if (jsonDoc.isObject())
        {
            QJsonObject jsonObject = jsonDoc.object();
            m_InfoSet = jsonObject["InfoSet"].toString();
            //
            QJsonArray jesarr = jsonObject["NameList"].toArray();
            QJsonArray jesarrColor = jsonObject["ClorDefine"].toArray();
            QJsonArray uncheckedDataSheetArray = jsonObject["UncheckedDataSheetList"].toArray();
            for (int i = 0; i < uncheckedDataSheetArray.size(); i++)
            {
                QString dataSheetName = uncheckedDataSheetArray[i].toString();
                if (!dataSheetName.isEmpty() && !m_UncheckedDataSheetList.contains(dataSheetName))
                {
                    m_UncheckedDataSheetList.append(dataSheetName);
                }
            }

            for (int i = 0; i < jesarr.size(); i++)
            {
                m_NameList.append(jesarr[i].toString());
                m_ClorDefine.append(QColor(QRgb(jesarrColor[i].toInt())));
            }
        }
    }
}

void YtYoloSetPro::toSaveData(QString Path)
{

    QFile file(QString("%1/%2.json").arg(Path).arg("DefineLabel"));
    if (!file.open(QIODevice::WriteOnly))
    {

        return;
    }
    // 创建一个JSON对象
    QJsonObject jsonObject;

    jsonObject.insert("InfoSet", m_InfoSet);

    QJsonArray jesarr;
    QJsonArray jesarrColor;
    QJsonArray uncheckedDataSheetArray;
    for (int i = 0; i < m_UncheckedDataSheetList.size(); i++)
    {
        uncheckedDataSheetArray.append(QJsonValue(m_UncheckedDataSheetList.at(i)));
    }
    for (int i = 0; i < m_NameList.size(); i++)
    {
        jesarr.append(QJsonValue(m_NameList.at(i)));
        jesarrColor.append(QJsonValue(int(m_ClorDefine.at(i).rgb())));
    }

    jsonObject.insert("NameList", jesarr);
    jsonObject.insert("ClorDefine", jesarrColor);
    jsonObject.insert("UncheckedDataSheetList", uncheckedDataSheetArray);
    // 创建一个JSON文档
    QJsonDocument jsonDoc(jsonObject);
    // 写入到文件
    file.write(jsonDoc.toJson(QJsonDocument::Indented));
    file.close();
    // 生成标签文件
    file.setFileName(QString("%1/%2.txt").arg(Path).arg("Class"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    for (int i = 0; i < m_NameList.size(); i++)
    {
        out << m_NameList.at(i) << "\n";
    }
    file.close();
}

void YtYoloSetPro::toSetInfoSet(QString InfoSet)
{
    m_InfoSet = InfoSet;
}

QString YtYoloSetPro::toGetInfoSet()
{
    return m_InfoSet;
}

void YtYoloSetPro::toSetLabelInfo(QVector<QString> NameList, QVector<QColor> ClorDefine)
{
    m_NameList = NameList;
    m_ClorDefine = ClorDefine;
}

void YtYoloSetPro::toAppendLabelInfo(QString NameList, QColor ClorDefine)
{
    m_NameList.append(NameList);
    m_ClorDefine.append(ClorDefine);
}

void YtYoloSetPro::toModifyInfo(QString NameList, QColor ClorDefine, int index)
{
    if (index >= 0 && index < m_NameList.size())
    {
        m_NameList[index] = NameList;
        m_ClorDefine[index] = ClorDefine;
    }
}

QVector<QString> YtYoloSetPro::toGetLabelName()
{
    return m_NameList;
}

QVector<QColor> YtYoloSetPro::toGetColorDefine()
{
    return m_ClorDefine;
}

ProTrainData::ProTrainData()
{
}

void ProTrainData::toSaveData(QString SavePath)
{
    QFile file(QString("%1/%2.json").arg(SavePath).arg("TrainSet"));
    if (!file.open(QIODevice::WriteOnly))
    {

        return;
    }
    // 创建一个JSON对象
    QJsonObject jsonObject;

    jsonObject.insert("ModelSize", m_ModelSize);
    jsonObject.insert("TaskType", m_TaskType);
    jsonObject.insert("EpochNum", m_EpochNum);
    jsonObject.insert("ImageSize", m_ImageSize);
    jsonObject.insert("MultiScal", m_isMultiScal);
    jsonObject.insert("Int8", m_Isint8);

    // int         m_WorksThread=8;//
    // int         m_BathSize=16;//
    // int         m_isMosick=10;//数据增强轮数
    jsonObject.insert("WorksThread", m_WorksThread);
    jsonObject.insert("BathSize", m_BathSize);
    jsonObject.insert("isMosick", m_isMosick);
    jsonObject.insert("SetImagechgle", m_SetImagechgle);

    // 创建一个JSON文档
    QJsonDocument jsonDoc(jsonObject);
    // 写入到文件
    file.write(jsonDoc.toJson(QJsonDocument::Indented));
    file.close();
}

void ProTrainData::toLoadData(QString LoadPath)
{
    QFile file(QString("%1/%2.json").arg(LoadPath).arg("TrainSet"));
    if (!file.open(QIODevice::ReadOnly))
    {

        return;
    }
    // 文件打开成功
    QByteArray jsonData = file.readAll();
    file.close();
    // 从QByteArray解析JSON文档
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    QJsonObject jsonObject = jsonDoc.object();
    m_ModelSize = jsonObject["ModelSize"].toInt();
    m_TaskType = jsonObject["TaskType"].toInt();
    m_EpochNum = jsonObject["EpochNum"].toInt();
    m_ImageSize = jsonObject["ImageSize"].toInt();
    m_isMultiScal = jsonObject["MultiScal"].toBool();
    m_Isint8 = jsonObject["Int8"].toBool();

    m_WorksThread = jsonObject["WorksThread"].toInt();
    m_BathSize = jsonObject["BathSize"].toInt();
    m_isMosick = jsonObject["isMosick"].toInt();
    m_SetImagechgle = jsonObject["SetImagechgle"].toInt();
}

int ProTrainData::toGetChangle()
{
    if (m_SetImagechgle != 1)
    {
        return 3;
    }
    return 1;
}

QString ProTrainData::toGetCmd()
{
    // epochs=30 imgsz=640 name=train exist_ok=true  multi_scale=true
    return QString("name=train exist_ok=true epochs=%1 imgsz=%2 multi_scale=%3 int8=%4 close_mosaic=%5 batch=%6 "
                   "workers=%7 amp=false cache=false")
        .arg(m_EpochNum)
        .arg(m_ImageSize)
        .arg(m_isMultiScal == true ? "true" : "false")
        .arg(m_Isint8 == true ? "true" : "false")
        .arg(m_isMosick)
        .arg(m_BathSize)
        .arg(m_WorksThread);
}

QString ProTrainData::toGetTrainCmd(QString processName, QString PretrainedModelPath)
{
    QStringList protask;
    protask << "detect" << "obb" << "segment" << "classify";
    QVector<QStringList> ProtrainMode;
    ProtrainMode << QStringList() << QStringList() << QStringList() << QStringList();
    //    Ytn,           //正矩形目标识别   0
    //    Yts,        //斜矩形目标识别   1 ,对应YOLO里的OBB(Oriented Bounding Box)概念
    //    Ytm,          //语义分割      2
    //    Ytl,          //语义分割      2
    //    Ytx,          //语义分割      2

    ProtrainMode[0] << "yolo11n.pt" << "yolo11s.pt" << "yolo11m.pt" << "yolo11l.pt" << "yolo11x.pt"
                    << "yolo11n-grayscale.pt" << "yolov5nu.pt" << "yolov6.yaml" << "yolov8n.pt" << "rtdetr-l.pt"
                    << "yolo12n.pt";
    ProtrainMode[1] << "yolo11n-obb.pt" << "yolo11s-obb.pt" << "yolo11m-obb.pt" << "yolo11l-obb.pt" << "yolo11x-obb.pt";
    ProtrainMode[2] << "yolo11n-seg.pt" << "yolo11s-seg.pt" << "yolo11m-seg.pt" << "yolo11l-seg.pt" << "yolo11x-seg.pt";
    ProtrainMode[3] << "yolo11n-cls.pt" << "yolo11s-cls.pt" << "yolo11m-cls.pt" << "yolo11l-cls.pt" << "yolo11x-cls.pt";

    if (m_ModelSize >= ProtrainMode[m_TaskType].size())
    {
        m_ModelSize = 0;
    }
    QString cmdDir;
    if (PretrainedModelPath.isEmpty() | PretrainedModelPath == "")
    {
        cmdDir = QString("%1/python.exe %2/Scripts/yolo.exe %3 train data=%4 model=%5/model/%6 %7 \r\n")
                     .arg(YtYoloDefine::toGetPythonPath())                                    // 1 运行目录
                     .arg(YtYoloDefine::toGetPythonPath())                                    // 2 运行目录
                     .arg(protask.at(m_TaskType))                                             // 3 训练模式
                     .arg(YtYoloDefine::toGetDataPath() + "/" + processName + "/Config.yaml") // 4 yml位置
                     .arg(YtYoloDefine::toGetPythonPath())                                    // 5 运行目录
                     .arg(ProtrainMode[m_TaskType][m_ModelSize])                              // 6 pt文件名称
                     .arg(toGetCmd())                                                         // 7 输入指令
            ;
        if (ProtrainMode[m_TaskType][m_ModelSize].contains("yaml"))
        {
            cmdDir = QString("%1/python.exe %2/Scripts/yolo.exe %3 train data=%4 model=%5/model/%6 %7 \r\n")
                         .arg(YtYoloDefine::toGetPythonPath())                                    // 1 运行目录
                         .arg(YtYoloDefine::toGetPythonPath())                                    // 2 运行目录
                         .arg("")                                                                 // 3 训练模式
                         .arg(YtYoloDefine::toGetDataPath() + "/" + processName + "/Config.yaml") // 4 yml位置
                         .arg(YtYoloDefine::toGetPythonPath())                                    // 5 运行目录
                         .arg(ProtrainMode[m_TaskType][m_ModelSize])                              // 6 pt文件名称
                         .arg(toGetCmd())                                                         // 7 输入指令
                ;
        }
    }
    else
    {

        cmdDir = QString("%1/python.exe %2/Scripts/yolo.exe %3 train data=%4 model=%5/model/%6 pretrained=%7 %8 \r\n")
                     .arg(YtYoloDefine::toGetPythonPath())                                    // 1 运行目录
                     .arg(YtYoloDefine::toGetPythonPath())                                    // 2 运行目录
                     .arg(protask.at(m_TaskType))                                             // 3 训练模式
                     .arg(YtYoloDefine::toGetDataPath() + "/" + processName + "/Config.yaml") // 4 yml位置
                     .arg(YtYoloDefine::toGetPythonPath())                                    // 5 运行目录
                     .arg(ProtrainMode[m_TaskType][m_ModelSize])                              // 6 pt文件名称
                     .arg(PretrainedModelPath) // 7 预训练模型文件path
                     .arg(toGetCmd())          // 8 输入指令
            ;
    }

    if (m_TaskType == 3)
    {
        cmdDir = QString("%1/python.exe %2/Scripts/yolo.exe %3 train data=%4 model=%5/model/%6 %7 \r\n")
                     .arg(YtYoloDefine::toGetPythonPath())                   // 1 运行目录
                     .arg(YtYoloDefine::toGetPythonPath())                   // 2 运行目录
                     .arg(protask.at(m_TaskType))                            // 3 训练模式
                     .arg(YtYoloDefine::toGetDataPath() + "/" + processName) // 4 yml位置
                     .arg(YtYoloDefine::toGetPythonPath())                   // 5 运行目录
                     .arg(ProtrainMode[m_TaskType][m_ModelSize])             // 6 pt文件名称
                     .arg(toGetCmd())                                        // 7 输入指令
            ;
    }
    return cmdDir;
}

QString ProTrainData::toGeExportCmd(QString processName)
{
    QStringList protask;
    protask << "detect" << "obb" << "segment" << "classify";
    //    QString cmdDir=QString("%1/python.exe %2/Scripts/yolo.exe export model=%3/%4/train/weights/best.pt format=onnx
    //    opset=12\r\n")
    //            .arg(YtYoloDefine::toGetPythonPath())//1 运行目录
    //            .arg(YtYoloDefine::toGetPythonPath())//2 运行目录;
    //            .arg(YtYoloDefine::toGetTrainPath()+"/"+processName)
    //            .arg(protask.at(m_TaskType));

    QString cmdDir =
        QString("%1/python.exe %2/Scripts/yolo.exe export model=%3/%4/train/weights/best.pt format=onnx opset=12\r\n")
            .arg(YtYoloDefine::toGetPythonPath()) // 1 运行目录
            .arg(YtYoloDefine::toGetPythonPath()) // 2 运行目录;
            .arg(YtYoloDefine::toGetTrainPath() + "/" + processName)
            .arg(protask.at(m_TaskType));

    if (m_TaskType != 2 || m_TaskType != 3)
    {
        cmdDir += QString("%1/python.exe %2/SetlProScrept/v8trans.py %3/%4/train/weights/best.onnx %5\r\n")
                      .arg(YtYoloDefine::toGetPythonPath()) // 1 运行目录
                      .arg(YtYoloDefine::toGetPythonPath()) // 2 运行目录;
                      .arg(YtYoloDefine::toGetTrainPath() + "/" + processName)
                      .arg(protask.at(m_TaskType))
                      .arg(processName);
    }

    //    if(m_TaskType==3)
    //    {
    //        //增加一个batch导出
    //        cmdDir=QString("%1/python.exe %2/Scripts/yolo.exe export model=%3/%4/train/weights/best.pt format=onnx
    //        dynamic=True opset=12\r\n")
    //                    .arg(YtYoloDefine::toGetPythonPath())//1 运行目录
    //                    .arg(YtYoloDefine::toGetPythonPath())//2 运行目录;
    //                    .arg(YtYoloDefine::toGetTrainPath()+"/"+processName)
    //                    .arg(protask.at(m_TaskType));
    //    }
    return cmdDir;
}

QString ProTrainData::toGeExportOpenVinoInt8Cmd(QString processName)
{
    QStringList protask;
    protask << "detect" << "obb" << "segment" << "classify";
    //    QString cmdDir=QString("%1/python.exe %2/Scripts/yolo.exe export model=%3/%4/train/weights/best.pt format=onnx
    //    opset=12\r\n")
    //            .arg(YtYoloDefine::toGetPythonPath())//1 运行目录
    //            .arg(YtYoloDefine::toGetPythonPath())//2 运行目录;
    //            .arg(YtYoloDefine::toGetTrainPath()+"/"+processName)
    //            .arg(protask.at(m_TaskType));

    QString cmdDir =
        QString("%1/python.exe %2/Scripts/yolo.exe export model=%3/%4/train/weights/best.pt format=openvino\r\n")
            .arg(YtYoloDefine::toGetPythonPath()) // 1 运行目录
            .arg(YtYoloDefine::toGetPythonPath()) // 2 运行目录;
            .arg(YtYoloDefine::toGetTrainPath() + "/" + processName)
            .arg(protask.at(m_TaskType));

    return cmdDir;
}

bool ProTrainData::IsJsonHasLabel(QString JsonPath, QString LabelName)
{
    QFile file(JsonPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }
    QByteArray jsonData = file.readAll();
    file.close();

    // 从QByteArray解析JSON文档
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);

    // 确保JSON文档是一个对象
    if (!jsonDoc.isObject())
    {
        return false;
    }
    //
    QJsonObject jsonObject = jsonDoc.object();
    //
    QJsonArray getlist = jsonObject["shapes"].toArray();
    //
    // qDebug()<<getlist<<getlist.size();
    bool isMatch = false;
    for (int i = 0; i < getlist.size(); i++)
    {
        QJsonObject tjsonObject = getlist[i].toObject();

        QString temLabelName = tjsonObject["label"].toString();
        if (temLabelName.compare(LabelName, Qt::CaseInsensitive) == 0)
        {
            isMatch = true;
            break;
        }
    }
    //    qDebug()<<"111 == 111:"<<QString(u8"111").compare("111");
    qDebug() << JsonPath << u8"/是否含有标签/" << LabelName << " :" << isMatch;
    return isMatch;
}

QString ProTrainData::toGeExportCmdBatch(QString processName, int BatchSize)
{
    QStringList protask;
    protask << "detect" << "obb" << "segment" << "classify";
    //    QString cmdDir=QString("%1/python.exe %2/Scripts/yolo.exe export model=%3/%4/train/weights/best.pt format=onnx
    //    opset=12\r\n")
    //            .arg(YtYoloDefine::toGetPythonPath())//1 运行目录
    //            .arg(YtYoloDefine::toGetPythonPath())//2 运行目录;
    //            .arg(YtYoloDefine::toGetTrainPath()+"/"+processName)
    //            .arg(protask.at(m_TaskType));

    QString cmdDir = QString("%1/python.exe %2/Scripts/yolo.exe export model=%3/%4/train/weights/best.pt format=onnx "
                             "opset=12 batch=%5\r\n")
                         .arg(YtYoloDefine::toGetPythonPath()) // 1 运行目录
                         .arg(YtYoloDefine::toGetPythonPath()) // 2 运行目录;
                         .arg(YtYoloDefine::toGetTrainPath() + "/" + processName)
                         .arg(protask.at(m_TaskType))
                         .arg(QString::number(BatchSize));

    //    if(m_TaskType!=2 || m_TaskType!=3)
    //    {
    //    cmdDir+=QString("%1/python.exe %2/SetlProScrept/v8trans.py %3/%4/train/weights/best.onnx %5\r\n")
    //            .arg(YtYoloDefine::toGetPythonPath())//1 运行目录
    //            .arg(YtYoloDefine::toGetPythonPath())//2 运行目录;
    //            .arg(YtYoloDefine::toGetTrainPath()+"/"+processName)
    //            .arg(protask.at(m_TaskType))
    //            .arg(processName+"Batch");
    //    }

    //    if(m_TaskType==3)
    //    {
    //        //增加一个batch导出
    //        cmdDir=QString("%1/python.exe %2/Scripts/yolo.exe export model=%3/%4/train/weights/best.pt format=onnx
    //        dynamic=True opset=12\r\n")
    //                    .arg(YtYoloDefine::toGetPythonPath())//1 运行目录
    //                    .arg(YtYoloDefine::toGetPythonPath())//2 运行目录;
    //                    .arg(YtYoloDefine::toGetTrainPath()+"/"+processName)
    //                    .arg(protask.at(m_TaskType));
    //    }
    return cmdDir;
}

QString ProTrainData::toGetQuantCmd(QString processName)
{
    QString QauntDataFolder =
        QFileDialog::getExistingDirectory(nullptr, u8"选择量化数据文件夹", YtYoloDefine::toGetLabelPath());
    QStringList protask;
    protask << "detect" << "obb" << "segment" << "classify";
    QString cmdDir =
        QString("%1/python.exe %2/Scripts/yolo.exe export model=%3/%4/train/weights/best.pt format=onnx opset=12\r\n")
            .arg(YtYoloDefine::toGetPythonPath()) // 1 运行目录
            .arg(YtYoloDefine::toGetPythonPath()) // 2 运行目录;
            .arg(YtYoloDefine::toGetTrainPath() + "/" + processName)
            .arg(protask.at(m_TaskType));

    cmdDir +=
        QString("%1/python.exe %2/SetlProScrept/quant_yolo_onnx.py --fp32_model_path %3/%4/train/weights/best.onnx "
                "--calibration_data_dir %5 --int8_model_dir %3/%4/train/weights/\r\n")
            .arg(YtYoloDefine::toGetPythonPath()) // 1 运行目录
            .arg(YtYoloDefine::toGetPythonPath()) // 2 运行目录;
            .arg(YtYoloDefine::toGetTrainPath() + "/" + processName)
            .arg(protask.at(m_TaskType))
            .arg(QauntDataFolder); // 量化数据集路径

    return cmdDir;
}

CMvRotatedRect toGetPRota(QVector<CMvPoint> GetPos)
{
    cv::Mat tsmdata;
    for (int i = 0; i < GetPos.size(); i++)
    {
        //
        tsmdata.push_back(cv::Point2f(GetPos[i].x, GetPos[i].y));
    }
    cv::RotatedRect t = cv::minAreaRect(tsmdata);
    CMvRotatedRect tout;
    tout.Center.x = t.center.x;
    tout.Center.y = t.center.y;
    qDebug() << t.angle << "PPPPPPZZZ";

    tout.cx = t.size.width / 2;
    tout.cy = t.size.height / 2;
    tout.angle = 360 - t.angle; /// MV_PI*180;

    return tout;
}
