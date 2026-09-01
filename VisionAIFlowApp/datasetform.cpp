#include "mlsdprocesser.h"
#include "datasetform.h"
#include "release/ui_datasetform.h"
#include <QTreeWidgetItem>
#include <QStyleFactory>
#include <QFileIconProvider>
#include <algorithm>
#include <random>
#include <QtConcurrent/QtConcurrent>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QStringConverter>
#include <QThreadPool>
DataSetForm::DataSetForm(QWidget *parent) : QWidget(parent), ui(new Ui::DataSetForm)
{
    ui->setupUi(this);
    ui->TW_LabeSet->verticalHeader()->setVisible(false); // 隐藏垂直表头
    ui->TW_LabeSet->horizontalHeader()->setStretchLastSection(true);
    ui->TW_LabeSet->setEditTriggers(QAbstractItemView::NoEditTriggers); // 不可编辑
    ui->TW_LabeSet->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->TW_LabeSet->setSelectionBehavior(QAbstractItemView::SelectRows); // 选择一行
    ui->TW_LabeSet->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->ytRoiShowDisp->toSetBGColor(QColor(33, 34, 35));
    ui->ToolTreeWidget->setIconSize(QSize(22, 22));
    ui->ToolTreeWidget->setStyle(QStyleFactory::create("windows"));
    ui->ToolTreeWidget->header()->setVisible(false);
    ui->ToolTreeWidget->clear();

    ui->ToolTreeWidget->header()->setVisible(false);
    ui->ToolTreeWidget->clear();
    connect(ui->ToolTreeWidget, &QTreeWidget::itemClicked, this, &DataSetForm::slot_ItemDoubleClicked);
    //
    ui->ytRoiShowDisp->addOverPlayPtr(&m_OverPlayShow, "set");
    connect(this, &DataSetForm::Sigfinish, this, &DataSetForm::slotFinish);
    connect(this, &DataSetForm::SigProcess, &m_ShowProcessForm, &ShowProcessForm::toSetProcess);
}

DataSetForm::~DataSetForm()
{
    delete ui;
}

void DataSetForm::toSetProcessName(QString Setname)
{
    m_ProcessName = Setname;
}

void DataSetForm::toInitShow()
{

    QDir temdir;
    temdir.setPath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName);
    //
    if (!temdir.exists())
    {
        temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName);
    }
    //
    toLoadData();
    toShowData();
}

QFileInfoList DataSetForm::toGetCheckedDataSheetDirList()
{
    QFileInfoList checkedDirList;
    QFileInfoList allDirList = YtYoloDefine::toGetPathDirInfo(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
    YtYoloSetPro setPro;
    setPro.toLoadData(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
    for (int i = 0; i < allDirList.size(); i++)
    {
        if (!setPro.m_UncheckedDataSheetList.contains(allDirList.at(i).fileName()))
        {
            checkedDirList.append(allDirList.at(i));
        }
    }
    return checkedDirList;
}
void DataSetForm::toProDataGet(bool isadd)
{
    qInfo() << "DataSetForm::toProDataGet begin"
            << "process" << m_ProcessName << "isadd" << isadd;
    m_YtYoloSetPro.toLoadData(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
    qInfo() << "DataSetForm::toProDataGet labels" << m_YtYoloSetPro.m_NameList.size() << m_YtYoloSetPro.m_NameList;
    const int runIndex = m_DataGenRunIndex;

    //
    m_TrainLabelSetfilename.clear(); // 按照类别进行文件存储
    m_ValLabelSetfilename.clear();   // 按照类别进行文件存储
    m_BackGroundJsonfilename.clear();
    m_BackGroudImgname.clear();

    //
    for (int i = 0; i < m_YtYoloSetPro.m_NameList.size(); i++)
    {
        m_TrainLabelSetfilename.append(QStringList());
        m_ValLabelSetfilename.append(QStringList());
        m_BackGroundJsonfilename.append(QStringList());
    }
    if (m_YtYoloSetPro.m_NameList.size() < 1)
    {
        m_TrainLabelSetfilename.append(QStringList());
        m_ValLabelSetfilename.append(QStringList());
        m_BackGroundJsonfilename.append(QStringList());
    }
    //
    QFileInfoList getDirpp = toGetCheckedDataSheetDirList();
    if (isadd)
    {
        getDirpp.append(QFileInfo(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/AddDataS"));
    }
    if (getDirpp.size() < 1)
    {
        m_DataGenErrorMessage = u8"未勾选任何标注目录, 无法生成数据集";
        qWarning() << "DataSetForm::toProDataGet no checked data sheet" << m_ProcessName;
        emit Sigfinish();
        return;
    }
    QDir temdir;
    qInfo() << "DataSetForm::toProDataGet run index" << runIndex;
    if (runIndex < 3)
    {
        temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/train/images");
        temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/train/labels");
        temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/val/images");
        temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/val/labels");
    }
    else
    {

        for (int i = 0; i < m_YtYoloSetPro.m_NameList.size(); i++)
        {
            temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/train/" +
                          m_YtYoloSetPro.m_NameList.at(i));
            temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/test/" +
                          m_YtYoloSetPro.m_NameList.at(i));
        }
        if (m_YtYoloSetPro.m_NameList.size() < 1)
        {
            temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/train");
        }
    }

    // m_ShowProcessForm.show();
    // 打开两个文本，用于存储csv
    QFile Trinfile(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/train.csv");
    QFile Valuefile(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/val.csv");
    bool isopen = Trinfile.open(QIODevice::WriteOnly | QIODevice::Text);
    bool isValOpen = Valuefile.open(QIODevice::WriteOnly | QIODevice::Text);
    qInfo() << "DataSetForm::toProDataGet csv open" << Trinfile.fileName() << isopen << Valuefile.fileName()
            << isValOpen;
    if (!isopen || !isValOpen)
    {
        m_DataGenErrorMessage = "Open train.csv or val.csv failed";
        qWarning() << "DataSetForm::toProDataGet csv open failed" << Trinfile.errorString() << Valuefile.errorString();
        emit Sigfinish();
        return;
    }

    QTextStream Trainout(&Trinfile);
    QTextStream Valout(&Valuefile);
    if (runIndex == 4 || runIndex == 5)
    {
        Trainout.setEncoding(QStringConverter::Utf8);
        Valout.setEncoding(QStringConverter::Utf8);
    }
    // 保存label文件
    QDir temdirSave;
    qInfo() << "DataSetForm::toProDataGet checked dirs" << getDirpp.size() << getDirpp;
    QFile temfile;

    // BG类别索引，用来判断是否有背景类别；
    int BG_label_index = -1;
    if (m_YtYoloSetPro.m_NameList.contains("background"))
    {
        BG_label_index = m_YtYoloSetPro.m_NameList.lastIndexOf("background");
    }
    qInfo() << "DataSetForm::toProDataGet background index" << BG_label_index;

    MlsdProcesser temMlsdProcesser;
    for (int i = 0; i < getDirpp.size(); i++)
    {
        emit SigProcess(100.0 * i / getDirpp.size());
        QFileInfo teminfor = getDirpp.at(i);
        qInfo() << "DataSetForm::toProDataGet dir begin" << i << teminfor.absoluteFilePath();

        //
        QStringList temlisfilte;
        temlisfilte << "*.json";
        QFileInfoList getfilelis = YtYoloDefine::toGetPathFileInfo(teminfor.absoluteFilePath(), temlisfilte);

        qInfo() << "DataSetForm::toProDataGet json count" << teminfor.absoluteFilePath() << getfilelis.size();
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(getfilelis.begin(), getfilelis.end(), g);

        //
        int minvalval = qMax(1.0, getfilelis.size() * 0.3); // 固定好测试集和训练集的处理

        temdirSave.setPath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/" + teminfor.fileName());
        if (!temdirSave.exists() && runIndex != 3)
        {
            temdirSave.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/" + teminfor.fileName());
        }

        for (int j = 0; j < getfilelis.size(); j++)
        {

            emit SigProcess(100.0 * i / getDirpp.size() + 100.0 / getDirpp.size() * j / getfilelis.size());

            const QString jsonPath = getfilelis.at(j).absoluteFilePath();
            qInfo() << "DataSetForm::toProDataGet json begin" << i << j << jsonPath;
            m_YtRoiLabelSet.toLoadJsonFile(jsonPath);
            qInfo() << "DataSetForm::toProDataGet json loaded" << jsonPath << "image" << m_YtRoiLabelSet.m_imagePath
                    << "labels" << m_YtRoiLabelSet.m_NameSetLis << "roi count" << m_YtRoiLabelSet.m_SetLabeset.size();

            // 负样本（背景图）在后面才加到训练集csv。
            // train.csv中，background类别样本一定要在train的其他类别样本后面，否则python端生成.cache那一步会报错。

            if (m_YtRoiLabelSet.m_NameSetLis.contains("background"))
            {
                if (BG_label_index < 0 || BG_label_index >= m_BackGroundJsonfilename.size())
                {
                    m_DataGenErrorMessage = "Json contains background, but project class list has no background";
                    qWarning() << "DataSetForm::toProDataGet invalid background label" << jsonPath << "bg index"
                               << BG_label_index << "groups" << m_BackGroundJsonfilename.size();
                    emit Sigfinish();
                    return;
                }
                m_BackGroudImgname.append(
                    QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath));
                m_BackGroundJsonfilename[BG_label_index].append(jsonPath);
                continue;
            }

            int FindexNameindex = m_YtRoiLabelSet.m_imagePath.lastIndexOf(".");
            const QString imagePath = QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath);
            if (m_YtRoiLabelSet.m_imagePath.isEmpty())
            {
                qWarning() << "DataSetForm::toProDataGet empty imagePath" << jsonPath;
                continue;
            }
            if (!QFile::exists(imagePath))
            {
                qWarning() << "DataSetForm::toProDataGet image missing" << jsonPath << imagePath;
                continue;
            }
            if (runIndex == 0)
            {
                // 矩形目标识别
                m_YtRoiLabelSet.toSaveDetcTxt(temdirSave.path(), m_YtYoloSetPro.m_NameList);
            }
            else if (runIndex == 1)
            {
                m_YtRoiLabelSet.toSaveObbTxt(temdirSave.path(), m_YtYoloSetPro.m_NameList);
            }
            else if (runIndex == 2)
            {
                m_YtRoiLabelSet.toSaveSegTxt(temdirSave.path(), m_YtYoloSetPro.m_NameList);
            }
            else if (runIndex == 3)
            {
                // 分类识别
                // m_YtRoiLabelSet.toSaveDetcTxt(temdirSave.path(),m_YtYoloSetPro.m_NameList);
            }
            else if (runIndex == 4)
            {
                // 字符识别
                // m_YtRoiLabelSet.toSaveDetcTxt(temdirSave.path(),m_YtYoloSetPro.m_NameList);
            }
            else if (runIndex == 5)
            {
                // 字符位置识别
                // m_YtRoiLabelSet.toSaveDetcTxt(temdirSave.path(),m_YtYoloSetPro.m_NameList);
                m_YtRoiLabelSet.toSaveFourPosTxt(temdirSave.path(), m_YtYoloSetPro.m_NameList);
            }
            if (runIndex == 4)
            {
                if (m_YtRoiLabelSet.m_SetLabeset.size() > 0 && m_YtRoiLabelSet.m_SetLabeset[0].Name.length() > 0)
                {
                    if (j < qMin(100, minvalval))
                    {
                        Valout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath) << "\t"
                               << QString("%1").arg(m_YtRoiLabelSet.m_SetLabeset[0].Name) << "\n";
                        m_ValLabelSetfilename[0].append(
                            QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath));
                    }
                    else
                    {
                        Trainout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath)
                                 << "\t" << QString("%1").arg(m_YtRoiLabelSet.m_SetLabeset[0].Name) << "\n";
                        m_TrainLabelSetfilename[0].append(
                            QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath));
                    }
                    //
                    QString getstr = m_YtRoiLabelSet.m_SetLabeset[0].Name;
                    for (int characterIndex = 0; characterIndex < getstr.length(); characterIndex++)
                    {
                        if (!m_GetKeys.contains(getstr.mid(characterIndex, 1)))
                        {
                            m_GetKeys.insert(getstr.mid(characterIndex, 1), getstr.mid(characterIndex, 1));
                        }
                    }
                }
            }
            else if (runIndex == 5)
            {
                if (j < qMin(100, minvalval))
                {
                    // 进入测试集
                    for (int z = 0; z < m_YtRoiLabelSet.m_NameSetLis.size(); z++)
                    {
                        int getsetindex = m_YtYoloSetPro.m_NameList.indexOf(m_YtRoiLabelSet.m_NameSetLis.at(z));
                        if (getsetindex >= 0)
                        {
                            m_ValLabelSetfilename[getsetindex].append(jsonPath);
                        }
                        else
                        {
                            qWarning() << "DataSetForm::toProDataGet unknown label" << jsonPath
                                       << m_YtRoiLabelSet.m_NameSetLis.at(z);
                        }
                    }
                    Valout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath) << " "
                           << QString("%1/%2.txt")
                                  .arg(temdirSave.path())
                                  .arg(m_YtRoiLabelSet.m_imagePath.left(FindexNameindex))
                           << "\n";
                }
                else
                {
                    // 进入训练集
                    for (int z = 0; z < m_YtRoiLabelSet.m_NameSetLis.size(); z++)
                    {
                        int getsetindex = m_YtYoloSetPro.m_NameList.indexOf(m_YtRoiLabelSet.m_NameSetLis.at(z));
                        if (getsetindex >= 0)
                        {
                            m_TrainLabelSetfilename[getsetindex].append(jsonPath);
                        }
                        else
                        {
                            qWarning() << "DataSetForm::toProDataGet unknown label" << jsonPath
                                       << m_YtRoiLabelSet.m_NameSetLis.at(z);
                        }
                    }
                    Trainout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath) << " "
                             << QString("%1/%2.txt")
                                    .arg(temdirSave.path())
                                    .arg(m_YtRoiLabelSet.m_imagePath.left(FindexNameindex))
                             << "\n";
                }
            }
            else if (runIndex == 6) // 直线mlsd数据集
            {
                // 测试集
                if (j < minvalval)
                {
                    temMlsdProcesser.m_ValidJsonObj.addSingleJson(getfilelis.at(j).absoluteFilePath(),
                                                                  teminfor.absoluteFilePath());

                    // ui列表数据
                    for (int z = 0; z < m_YtRoiLabelSet.m_NameSetLis.size(); z++)
                    {
                        int getsetindex = m_YtYoloSetPro.m_NameList.indexOf(m_YtRoiLabelSet.m_NameSetLis.at(z));
                        if (getsetindex >= 0)
                        {
                            m_ValLabelSetfilename[getsetindex].append(jsonPath);
                        }
                        else
                        {
                            qWarning() << "DataSetForm::toProDataGet unknown label" << jsonPath
                                       << m_YtRoiLabelSet.m_NameSetLis.at(z);
                        }
                    }
                    Valout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath) << " "
                           << QString("%1/%2.txt")
                                  .arg(temdirSave.path())
                                  .arg(m_YtRoiLabelSet.m_imagePath.left(FindexNameindex))
                           << "\n";
                }

                // 训练集
                else
                {
                    temMlsdProcesser.m_TrainJsonObj.addSingleJson(getfilelis.at(j).absoluteFilePath(),
                                                                  teminfor.absoluteFilePath());

                    for (int z = 0; z < m_YtRoiLabelSet.m_NameSetLis.size(); z++)
                    {
                        int getsetindex = m_YtYoloSetPro.m_NameList.indexOf(m_YtRoiLabelSet.m_NameSetLis.at(z));
                        if (getsetindex >= 0)
                        {
                            m_TrainLabelSetfilename[getsetindex].append(jsonPath);
                        }
                        else
                        {
                            qWarning() << "DataSetForm::toProDataGet unknown label" << jsonPath
                                       << m_YtRoiLabelSet.m_NameSetLis.at(z);
                        }
                    }
                    Trainout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath) << " "
                             << QString("%1/%2.txt")
                                    .arg(temdirSave.path())
                                    .arg(m_YtRoiLabelSet.m_imagePath.left(FindexNameindex))
                             << "\n";
                }
            }
            else
            {
                //
                if (j < minvalval)
                {
                    // 进入测试集
                    for (int z = 0; z < m_YtRoiLabelSet.m_NameSetLis.size(); z++)
                    {
                        int getsetindex = m_YtYoloSetPro.m_NameList.indexOf(m_YtRoiLabelSet.m_NameSetLis.at(z));
                        if (getsetindex >= 0)
                        {
                            m_ValLabelSetfilename[getsetindex].append(jsonPath);
                        }
                        else
                        {
                            qWarning() << "DataSetForm::toProDataGet unknown label" << jsonPath
                                       << m_YtRoiLabelSet.m_NameSetLis.at(z);
                        }
                        if (runIndex == 3)
                        {
                            QFile::copy(getfilelis.at(j).path() + "/" + m_YtRoiLabelSet.m_imagePath,
                                        QString("%1/test/%2/%3_%4")
                                            .arg(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName)
                                            .arg(m_YtRoiLabelSet.m_NameSetLis[z])
                                            .arg(i)
                                            .arg(m_YtRoiLabelSet.m_imagePath));
                        }
                    }
                    Valout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath) << ","
                           << QString("%1/%2.txt")
                                  .arg(temdirSave.path())
                                  .arg(m_YtRoiLabelSet.m_imagePath.left(FindexNameindex))
                           << "\n";
                }
                else
                {
                    // 进入训练集
                    for (int z = 0; z < m_YtRoiLabelSet.m_NameSetLis.size(); z++)
                    {
                        int getsetindex = m_YtYoloSetPro.m_NameList.indexOf(m_YtRoiLabelSet.m_NameSetLis.at(z));
                        if (getsetindex >= 0)
                        {
                            m_TrainLabelSetfilename[getsetindex].append(jsonPath);
                        }
                        else
                        {
                            qWarning() << "DataSetForm::toProDataGet unknown label" << jsonPath
                                       << m_YtRoiLabelSet.m_NameSetLis.at(z);
                        }
                        if (runIndex >= 3)
                        {
                            QFile::copy(getfilelis.at(j).path() + "/" + m_YtRoiLabelSet.m_imagePath,
                                        QString("%1/train/%2/%3_%4")
                                            .arg(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName)
                                            .arg(m_YtRoiLabelSet.m_NameSetLis[z])
                                            .arg(i)
                                            .arg(m_YtRoiLabelSet.m_imagePath));
                        }
                    }
                    Trainout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath) << ","
                             << QString("%1/%2.txt")
                                    .arg(temdirSave.path())
                                    .arg(m_YtRoiLabelSet.m_imagePath.left(FindexNameindex))
                             << "\n";
                }
            }
        }
    }

    if (runIndex == 6)
    {
        temMlsdProcesser.m_TrainJsonObj.toSaveColletedJson(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName +
                                                           "/train.json");
        temMlsdProcesser.m_ValidJsonObj.toSaveColletedJson(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName +
                                                           "/val.json");
    }
    // 增加负样本到训练集
    for (int j = 0; j < m_BackGroudImgname.size(); j++)
    {
        Trainout << m_BackGroudImgname.at(j) << ',' << QString("SIGNAL_BACKGROUND_IMG") << "\n";
    }

    qInfo() << "DataSetForm::toProDataGet finish"
            << "train groups" << m_TrainLabelSetfilename.size() << "val groups" << m_ValLabelSetfilename.size() << "bg"
            << m_BackGroudImgname.size();
    Trinfile.close();
    Valuefile.close();

    emit Sigfinish();
}

void DataSetForm::toShowData()
{
    qDebug() << "toshow 11111111";
    if (m_YtYoloSetPro.m_NameList.size() == 0 && ui->comboBox->currentIndex() == 4)
    {
        ui->TW_LabeSet->setRowCount(1);
        // 序号
        ui->TW_LabeSet->setItem(0, 0, new QTableWidgetItem(QString::number(1)));
        // 名称
        ui->TW_LabeSet->setItem(0, 1, new QTableWidgetItem(u8"数据集"));
        // 全部初始化为0
        ui->TW_LabeSet->setItem(0, 2, new QTableWidgetItem(QString::number(m_TrainLabelSetfilename[0].size())));
        ui->TW_LabeSet->setItem(0, 3, new QTableWidgetItem(QString::number(m_ValLabelSetfilename[0].size())));
        ui->TW_LabeSet->setItem(0, 4, new QTableWidgetItem(QString::number(0)));
        ui->ToolTreeWidget->clear();
        //
        QTreeWidgetItem *itemParent = nullptr;
        QFileIconProvider ticoset;

        itemParent = new QTreeWidgetItem(ui->ToolTreeWidget);
        itemParent->setText(0, u8"字符集");
        itemParent->setFlags(Qt::NoItemFlags | Qt::ItemIsEnabled);
        itemParent->setIcon(0, ticoset.icon(QFileIconProvider::Folder));
        for (int j = 0; j < m_TrainLabelSetfilename[0].size(); j++)
        {
            QTreeWidgetItem *treeItem = new QTreeWidgetItem(itemParent, QStringList(m_TrainLabelSetfilename[0][j]));
            treeItem->setIcon(0, ticoset.icon(QFileIconProvider::File));
        }

        for (int j = 0; j < m_ValLabelSetfilename[0].size(); j++)
        {
            QTreeWidgetItem *treeItem = new QTreeWidgetItem(itemParent, QStringList(m_ValLabelSetfilename[0][j]));
            treeItem->setIcon(0, ticoset.icon(QFileIconProvider::Network));
        }
        for (int i = 0; i < 1; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                if (ui->TW_LabeSet->item(i, j))
                {
                    ui->TW_LabeSet->item(i, j)->setTextAlignment(Qt::AlignCenter);
                }
            }
        }

        ui->ToolTreeWidget->expandAll();

        return;
    }

    // 开始构造表格
    ui->TW_LabeSet->setRowCount(m_YtYoloSetPro.m_NameList.size());

    for (int i = 0; i < m_YtYoloSetPro.m_NameList.size(); i++)
    {
        qDebug() << "toshow 11111111-i" << i;
        // 序号
        ui->TW_LabeSet->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        // 名称
        ui->TW_LabeSet->setItem(i, 1, new QTableWidgetItem(m_YtYoloSetPro.m_NameList.at(i)));
        // 全部初始化为0
        if (i >= m_TrainLabelSetfilename.size())
        {
            qDebug() << "toshow 11111111-2" << i;
            ui->TW_LabeSet->setItem(i, 2, new QTableWidgetItem(QString::number(0)));
            ui->TW_LabeSet->setItem(i, 3, new QTableWidgetItem(QString::number(0)));
            ui->TW_LabeSet->setItem(i, 4, new QTableWidgetItem(QString::number(0)));
        }
        else
        {
            qDebug() << "toshow 11111111-3" << i;
            if (i < m_TrainLabelSetfilename.size())
            {
                ui->TW_LabeSet->setItem(i,
                                        2,
                                        new QTableWidgetItem(QString::number(m_TrainLabelSetfilename.at(i).size())));
            }
            if (i < m_ValLabelSetfilename.size())
            {
                ui->TW_LabeSet->setItem(i,
                                        3,
                                        new QTableWidgetItem(QString::number(m_ValLabelSetfilename.at(i).size())));
            }
            if (i < m_BackGroundJsonfilename.size())
            {
                ui->TW_LabeSet->setItem(i,
                                        4,
                                        new QTableWidgetItem(QString::number(m_BackGroundJsonfilename.at(i).size())));
            }

            //            //增加background数量
            //            if(m_YtYoloSetPro.m_NameList.contains("background"))
            //            {
            //                int BG_label_index = m_YtYoloSetPro.m_NameList.lastIndexOf("background");
            //                qDebug()<<"show data row: "<<i<<" ;index of BG_label_index: "<<BG_label_index;
            //                ui->TW_LabeSet->setItem(i,4,new
            //                QTableWidgetItem(QString::number(m_BackGroundJsonfilename.at(i).size())));
            //            }
        }
    }

    //    for(int i=0;i<m_YtYoloSetPro.m_NameList.size();i++)
    //    {
    //        for(int j=0;j<5;j++)
    //        {
    //            qDebug()<<"toshow 11111111-4"<<i<<j;
    //            ui->TW_LabeSet->item(i,j)->setTextAlignment(Qt::AlignCenter);
    //        }
    //    }

    ui->ToolTreeWidget->clear();
    //
    QTreeWidgetItem *itemParent = nullptr;
    QFileIconProvider ticoset;
    for (int i = 0; i < m_YtYoloSetPro.m_NameList.size(); i++)
    {
        itemParent = new QTreeWidgetItem(ui->ToolTreeWidget);
        itemParent->setText(0, m_YtYoloSetPro.m_NameList.at(i));
        itemParent->setFlags(Qt::NoItemFlags | Qt::ItemIsEnabled);
        itemParent->setIcon(0, ticoset.icon(QFileIconProvider::Folder));
        if (i < m_TrainLabelSetfilename.size())
        {
            for (int j = 0; j < m_TrainLabelSetfilename[i].size(); j++)
            {
                QTreeWidgetItem *treeItem = new QTreeWidgetItem(itemParent, QStringList(m_TrainLabelSetfilename[i][j]));
                treeItem->setIcon(0, ticoset.icon(QFileIconProvider::File));
            }
        }
        if (i < m_ValLabelSetfilename.size())
        {
            for (int j = 0; j < m_ValLabelSetfilename[i].size(); j++)
            {
                QTreeWidgetItem *treeItem = new QTreeWidgetItem(itemParent, QStringList(m_ValLabelSetfilename[i][j]));
                treeItem->setIcon(0, ticoset.icon(QFileIconProvider::Network));
            }
        }
        if (i < m_BackGroundJsonfilename.size())
        {
            for (int j = 0; j < m_BackGroundJsonfilename[i].size(); j++)
            {
                QTreeWidgetItem *treeItem =
                    new QTreeWidgetItem(itemParent, QStringList(m_BackGroundJsonfilename[i][j]));
                treeItem->setIcon(0, ticoset.icon(QFileIconProvider::File));
            }
        }
    }
    ui->ToolTreeWidget->expandAll();
}

void DataSetForm::toSaveData(int changele, bool datapp)
{
    QFile file(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/LablData.data");
    if (!file.open(QIODevice::WriteOnly))
    {
        return;
    }
    QDataStream out(&file);

    out << m_BackGroundJsonfilename << m_TrainLabelSetfilename << m_ValLabelSetfilename << ui->comboBox->currentIndex();

    file.close();
    // 尝试存储yml文件
    QFile filet(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/Config.yaml");
    if (!filet.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }
    QTextStream outt(&filet);
    outt.setEncoding(QStringConverter::Utf8);

    outt << QString("path: %1/").arg(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName) << "\n";
    outt << QString("train: %1").arg("train") << "\n";
    outt << QString("val: %1").arg("val") << "\n";

    outt << "names:"
         << "\n";
    for (int i = 0; i < m_YtYoloSetPro.m_NameList.size(); i++)
    {
        outt << QString("  %1: %2").arg(i).arg(m_YtYoloSetPro.m_NameList.at(i)) << "\n";
    }
    outt << QString("channels: %1").arg(changele) << "\n";
    if (datapp)
    {
        outt << "augment:"
             << "\n";
        outt << QString("  %1: %2").arg("flipud").arg(0.5) << "\n";
        outt << QString("  %1: %2").arg("fliplr").arg(0.5) << "\n";
        outt << QString("  %1: %2").arg("mosaic").arg(0.5) << "\n";
        outt << QString("  %1: %2").arg("mixup").arg(0.5) << "\n";
        outt << QString("  %1: %2").arg("hsv_h").arg(0.015) << "\n";
        outt << QString("  %1: %2").arg("hsv_s").arg(0.7) << "\n";
        outt << QString("  %1: %2").arg("hsv_v").arg(0.4) << "\n";
        outt << QString("  %1: %2").arg("scale").arg(0.5) << "\n";
        outt << QString("  %1: %2").arg("shear").arg(0.1) << "\n";
        outt << QString("  %1: %2").arg("perspective").arg(0.1) << "\n";
    }
    filet.close();
    // 保存keys
    if (m_GetKeys.size() < 1)
    {
        return;
    }
    QFile files(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/Keys.txt");
    if (!files.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }
    QTextStream outs(&files);
    outs.setEncoding(QStringConverter::Utf8);
    QList<QString> keylist = m_GetKeys.keys();
    for (int i = 0; i < keylist.size(); i++)
    {
        outs << keylist[i] << "\n";
    }

    files.close();
}

void DataSetForm::toLoadData()
{
    m_YtYoloSetPro.toLoadData(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
    m_BackGroundJsonfilename.clear();
    m_TrainLabelSetfilename.clear();
    m_ValLabelSetfilename.clear();

    const QString dataFilePath = YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/LablData.data";
    QFile file(dataFilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "DataSetForm::toLoadData open failed" << dataFilePath << file.errorString();
        return;
    }

    QVector<QStringList> backGroundJsonfilename;
    QVector<QStringList> trainLabelSetfilename;
    QVector<QStringList> valLabelSetfilename;
    int value = -1;
    QDataStream out(&file);
    out >> backGroundJsonfilename >> trainLabelSetfilename >> valLabelSetfilename >> value;
    if (out.status() != QDataStream::Ok)
    {
        qWarning() << "DataSetForm::toLoadData stream failed" << dataFilePath << out.status();
        file.close();
        return;
    }
    file.close();

    if (value < 0 || value >= ui->comboBox->count())
    {
        qWarning() << "DataSetForm::toLoadData invalid run index" << dataFilePath << value;
        value = 0;
    }

    m_BackGroundJsonfilename = backGroundJsonfilename;
    m_TrainLabelSetfilename = trainLabelSetfilename;
    m_ValLabelSetfilename = valLabelSetfilename;
    ui->comboBox->setCurrentIndex(value);
    qInfo() << "DataSetForm::toLoadData loaded" << dataFilePath << "train groups" << m_TrainLabelSetfilename.size()
            << "val groups" << m_ValLabelSetfilename.size() << "run index" << value;
}

QString DataSetForm::toGetRunType()
{

    if (m_TrainLabelSetfilename.size() < 1)
    {
        return u8"目标识别";
    }
    return ui->comboBox->currentText();
}

int DataSetForm::toGetRunIndex()
{
    toLoadData();
    if (m_TrainLabelSetfilename.size() < 1)
    {
        return -1;
    }
    return ui->comboBox->currentIndex();
}

void DataSetForm::on_PB_RunDataGen_clicked()
{
    if (toGetCheckedDataSheetDirList().size() < 1)
    {
        qWarning() << u8"未勾选任何标注目录, 无法生成数据集";
        QMessageBox::warning(0, u8"警告", u8"未勾选任何标注目录, 无法生成数据集", u8"确定");
        return;
    }
    QDir temdirPlus;
    temdirPlus.setPath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/AddDataS");
    if (temdirPlus.exists())
    {
        qDebug() << temdirPlus.path();
        temdirPlus.removeRecursively();
    }
    temdirPlus.setPath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName);
    if (temdirPlus.exists())
    {
        qDebug() << temdirPlus.path();
        temdirPlus.removeRecursively();
    }
    // toProDataGet();
    m_DataGenErrorMessage.clear();
    m_DataGenRunIndex = ui->comboBox->currentIndex();
    qInfo() << "DataSetForm::on_PB_RunDataGen_clicked start"
            << "process" << m_ProcessName << "run index" << m_DataGenRunIndex;
    m_GetKeys.clear();
    QThreadPool::globalInstance()->start(
        [this]()
        {
            toProDataGet(false);
        });
    m_ShowProcessForm.exec();
    // QMessageBox::about(0,u8"提示",u8"数据生成完成!");
}

void DataSetForm::on_PB_ViewPos_clicked()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName));
}

void DataSetForm::slot_ItemDoubleClicked(QTreeWidgetItem *item, int colum)
{

    if (ui->comboBox->currentIndex() == 4)
    {

        m_OverPlayShow.toClearData();
        int getindex = item->text(0).lastIndexOf(".");
        m_YtRoiLabelSet.toLoadJsonFile(item->text(0).left(getindex) + ".json");
        QImage temim = QImage(item->text(0));
        qDebug() << item->text(0).left(getindex) + ".json" << m_YtRoiLabelSet.m_SetLabeset.size();

        if (m_YtRoiLabelSet.m_SetLabeset.size() > 0)
        {

            m_OverPlayShow.append(DispTxt(m_YtRoiLabelSet.m_SetLabeset[0].Name,
                                          CMvPoint(0, temim.height()),
                                          QFont("Times", 33, 33),
                                          Qt::darkGreen));
        }
        ui->ytRoiShowDisp->toSetImage(temim);
        ui->ytRoiShowDisp->toUpdateShow();

        return;
    }

    if (!item->text(0).contains("json"))
    {
        return;
    }

    //
    m_YtRoiLabelSet.toLoadJsonFile(item->text(0));
    //
    int getindex = item->text(0).lastIndexOf("/");
    //
    if (ui->comboBox->currentIndex() == 0)
    {
        m_YtRoiLabelSet.toGenDetcOveplay(m_OverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
    }
    else if (ui->comboBox->currentIndex() == 1)
    {
        m_YtRoiLabelSet.toGenObbOveplay(m_OverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
    }
    else if (ui->comboBox->currentIndex() == 2)
    {
        m_YtRoiLabelSet.toGenSegOveplay(m_OverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
    }
    else if (ui->comboBox->currentIndex() == 5)
    {
        m_YtRoiLabelSet.toGenFourPosOveplay(m_OverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
    }
    ui->ytRoiShowDisp->toSetImage(QImage(item->text(0).left(getindex) + "/" + m_YtRoiLabelSet.m_imagePath));
    ui->ytRoiShowDisp->toUpdateShow();
}

void DataSetForm::slotFinish()
{
    m_ShowProcessForm.close();
    if (!m_DataGenErrorMessage.isEmpty())
    {
        QMessageBox::warning(0, u8"警告", m_DataGenErrorMessage, u8"确定");
        m_DataGenErrorMessage.clear();
        return;
    }
    toShowData();
    toSaveData();
    QMessageBox::about(0, u8"提示", u8"数据生成完成!");
}
void DataSetForm::on_PB_RunDataAppend_clicked()
{

    return;
    if (m_TrainLabelSetfilename.size() > 100)
    {
        QMessageBox::about(0, u8"提示", u8"数据已经足够!");

        return;
    }

    QImage temimage;
    QSize temsize;
    QDir temdirPlus;
    temdirPlus.setPath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/AddDataS");
    temdirPlus.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/AddDataS");
    int pzeYY = 0;
    for (int i = 0; i < m_TrainLabelSetfilename.size(); i++)
    {
        for (int j = 0; j < m_TrainLabelSetfilename[i].size(); j++)
        {
            QString getStr = m_TrainLabelSetfilename[i][j];
            m_YtRoiLabelSet.toLoadJsonFile(getStr);
            int getindex = getStr.lastIndexOf("/");
            //
            temimage.load(getStr.left(getindex) + "/" + m_YtRoiLabelSet.m_imagePath);
            temsize = temimage.size();
            for (int zx = -35; zx <= 35; zx += 10)
            {
                for (int zy = -35; zy < 35; zy += 10)
                {
                    QString filename = temdirPlus.path() + "/" + QString::number(pzeYY) + ".jpg";
                    qDebug() << filename << zx << zy;
                    temimage.copy(zx, zy, temsize.width(), temsize.height()).save(filename);
                    //
                    m_YtRoiLabelSet.toSetFileName(QString::number(pzeYY) + ".jpg");
                    m_YtRoiLabelSet.toMovePos(-zx, -zy);

                    m_YtRoiLabelSet.toSaveJson(temdirPlus.path());

                    m_YtRoiLabelSet.toMovePos(zx, zy);
                    pzeYY += 1;
                }
            }
        }
    }

    for (int i = 0; i < m_ValLabelSetfilename.size(); i++)
    {
        for (int j = 0; j < m_ValLabelSetfilename[i].size(); j++)
        {
            QString getStr = m_ValLabelSetfilename[i][j];
            m_YtRoiLabelSet.toLoadJsonFile(getStr);
            int getindex = getStr.lastIndexOf("/");
            //
            temimage.load(getStr.left(getindex) + "/" + m_YtRoiLabelSet.m_imagePath);
            temsize = temimage.size();
            for (int zx = -35; zx <= 35; zx += 10)
            {
                for (int zy = -35; zy < 35; zy += 10)
                {
                    QString filename = temdirPlus.path() + "/" + QString::number(pzeYY) + ".jpg";
                    qDebug() << filename << zx << zy;
                    temimage.copy(zx, zy, temsize.width(), temsize.height()).save(filename);
                    //
                    m_YtRoiLabelSet.toSetFileName(QString::number(pzeYY) + ".jpg");
                    m_YtRoiLabelSet.toMovePos(-zx, -zy);
                    m_YtRoiLabelSet.toSaveJson(temdirPlus.path());
                    m_YtRoiLabelSet.toMovePos(zx, zy);
                    pzeYY += 1;
                }
            }
        }
    }
    //
    toProDataGet(true);
    toShowData();

    QMessageBox::about(0, u8"提示", u8"数据生成完成!");
}
