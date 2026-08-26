#include "valsetform.h"
#include "release/ui_valsetform.h"
#include "filecopysetdlg.h"
#include <QFileDialog>
#include <QtConcurrent/QtConcurrent>
#include <QStringConverter>
#include <QThreadPool>
ValSetForm::ValSetForm(QWidget *parent) : QWidget(parent), ui(new Ui::ValSetForm)
{
    ui->setupUi(this);
    ui->ytRoiShowDisp->addOverPlayPtr(&m_OverPlayShow, "set");
    ui->ytRoiShowDispJson->addOverPlayPtr(&m_JsonOverPlayShow, "set");
    ui->ytRoiShowDisp->toSetBGColor(QColor(33, 34, 35));
    ui->ytRoiShowDispJson->toSetBGColor(QColor(33, 34, 35));
}

ValSetForm::~ValSetForm()
{
    if (m_SetProcess)
    {
        m_SetProcess->close();
        delete m_SetProcess;
        m_SetProcess = nullptr;
    }
    delete ui;
}

void ValSetForm::toSetProcessName(QString Setname)
{
    m_ProcessName = Setname;
    ui->LE_CurentProName->setText(m_ProcessName);
}

void ValSetForm::toInitShow()
{
    ui->LE_ModeType->setText(m_DataSetForm->toGetRunType());
    //
    QDir temdir;
    temdir.setPath(YtYoloDefine::toGetValuePath() + "/" + m_ProcessName);
    if (!temdir.exists())
    {
        temdir.mkpath(YtYoloDefine::toGetValuePath() + "/" + m_ProcessName);
    }
    //
    m_YtYoloSetPro.toLoadData(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
    toLoadCsv();
}

void ValSetForm::on_PB_AddFiles_clicked()
{
    FileCopySetDlg tDlg;
    tDlg.setWindowFlags(Qt::SubWindow);
    if (QDialog::Accepted == tDlg.exec())
    {
        ui->listWidget->addItems(tDlg.toGetFileList());
    }
    toSaveCsv();
}

void ValSetForm::on_PB_Clear_clicked()
{
    ui->listWidget->clear();
    toSaveCsv();
}

void ValSetForm::toSaveCsv()
{
    QFile Trinfile(YtYoloDefine::toGetValuePath() + "/" + m_ProcessName + "/ProFile.csv");
    Trinfile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream Trainout(&Trinfile);
    //
    Trainout.setEncoding(QStringConverter::Utf8);
    for (int i = 0; i < ui->listWidget->count(); i++)
    {
        Trainout << ui->listWidget->item(i)->text() << "\n";
    }
    Trinfile.close();
}

void ValSetForm::toLoadCsv()
{
    ui->listWidget->clear();
    QFile file(YtYoloDefine::toGetValuePath() + "/" + m_ProcessName + "/ProFile.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    while (!in.atEnd())
    {
        QString line = in.readLine();
        // 处理每行文本
        ui->listWidget->addItem(line);
    }

    file.close();
}

void ValSetForm::toDoRunPP()
{
    // 这里只有遍历 txt文件进行转存即可
    for (int i = 0; i < ui->listWidget->count(); i++)
    {
        m_Getfilename = ui->listWidget->item(i)->text();
        //
        int getindex = m_Getfilename.lastIndexOf(".");
        if (getindex < 0)
        {
            continue;
        }
        m_YtRoiLabelSet.toClearData();
        QFile tsetfile;
        tsetfile.setFileName(m_Getfilename.left(getindex) + ".txt");
        if (tsetfile.exists())
        {
            switch (m_DataSetForm->toGetRunIndex())
            {
            case 0:
            {
                // 矩形目标识别
                m_YtRoiLabelSet.toLoadDetcTxt(m_Getfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);

                break;
            }
            case 1:
            {
                // OBB识别
                m_YtRoiLabelSet.toLoadObbTxt(m_Getfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);

                break;
            }
            case 2:
            {
                // 语义分割
                m_YtRoiLabelSet.toLoadSegTxt(m_Getfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);

                break;
            }
            case 3:
            {
                // 分类识别
                m_YtRoiLabelSet.toLoadClassTxt(m_Getfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);
                break;
            }
            default:
                break;
            }
            for (int j = 0; j < m_YtRoiLabelSet.m_SetLabeset.size(); j++)
            {
                QDir SaveFilePath(m_GetSavePath + "/" + m_YtRoiLabelSet.m_SetLabeset[j].toGetName());
                if (!SaveFilePath.exists())
                {
                    SaveFilePath.mkpath(m_GetSavePath + "/" + m_YtRoiLabelSet.m_SetLabeset[j].toGetName());
                }

                if (!QFile::exists(SaveFilePath.path() + "/" + m_Getfilename.split("\\").last()))
                {
                    //                    ui->plainTextEdit_Log->append(m_Getfilename);
                    //                    ui->plainTextEdit_Log->append(SaveFilePath.path()+"/"+m_Getfilename.split("\\").last());
                    //                    ui->plainTextEdit_Log->update();

                    QFile::copy(m_Getfilename, SaveFilePath.path() + "/" + m_Getfilename.split("\\").last());
                }
            }
        }
    }
    ui->plainTextEdit_Log->append(u8"执行结束");
    ui->plainTextEdit_Log->update();
}

void ValSetForm::toDoRunOCR(QString savepth)
{
    // CMvRotatedRect toGetPRota(QVector<CMvPoint> GetPos);
    CMvRotatedRect temrect;
    QVector<CMvPoint> temposints;
    // 这里只有遍历 txt文件进行转存即可
    for (int i = 0; i < ui->listWidget->count(); i++)
    {
        m_Getfilename = ui->listWidget->item(i)->text();
        //
        int getindex = m_Getfilename.lastIndexOf(".");
        if (getindex < 0)
        {
            continue;
        }
        m_YtRoiLabelSet.toClearData();
        QFile tsetfile;
        tsetfile.setFileName(m_Getfilename.left(getindex) + ".json");
        if (tsetfile.exists())
        {
            m_YtRoiLabelSet.toLoadJsonFile(tsetfile.fileName());
            m_GetImage = QImage(m_Getfilename).convertToFormat(QImage::Format_RGB888);

            QImage temp = m_GetImage.copy();
            memset(temp.bits(), 0, temp.sizeInBytes());
            for (int j = 0; j < m_YtRoiLabelSet.m_SetLabeset.size(); j++)
            {
                // qDebug()<<i<<j<<m_YtRoiLabelSet.m_SetLabeset[j].toGetPoints();
                // 用点集去计算斜矩形，然后裁切
                QVector<QPointF> pp = m_YtRoiLabelSet.m_SetLabeset[j].toGetPoints();
                temposints.clear();
                for (int z = 0; z < pp.size(); z++)
                {
                    temposints.append(CMvPoint(pp[z].x(), pp[z].y()));
                }
                temrect = toGetPRota(temposints);
                // 先对qimage 进行仿射变换
                QPainter painter(&temp);
                painter.translate(temrect.Center.x, temrect.Center.y); // 使图片的中心作为旋转的中心
                if (temrect.cx > temrect.cy)
                {
                    painter.rotate(temrect.angle); // 顺时针旋转90°
                }
                else
                {
                    painter.rotate(temrect.angle + 90); // 顺时针旋转90°
                }
                painter.translate(-temrect.Center.x, -temrect.Center.y); // 将原点复位
                painter.drawImage(QRect(0, 0, temp.width(), temp.height()), m_GetImage);
                //
                qDebug() << savepth + "/" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz") + ".jpg";
                if (temrect.cx > temrect.cy)
                {
                    temp.copy(temrect.Center.x - int(temrect.cx + 3) / 4 * 4,
                              temrect.Center.y - temrect.cy,
                              int(temrect.cx + 3) / 4 * 8,
                              2 * temrect.cy)
                        .save(savepth + "/" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz") + ".jpg");
                }
                else
                {
                    temp.copy(temrect.Center.x - int(temrect.cy + 3) / 4 * 4,
                              temrect.Center.y - temrect.cx,
                              int(temrect.cy + 3) / 4 * 8,
                              2 * temrect.cx)
                        .save(savepth + "/" + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz") + ".jpg");
                }
            }
        }
    }
    ui->plainTextEdit_Log->append(u8"执行结束");
    ui->plainTextEdit_Log->update();
}

void ValSetForm::read_data()
{
    // 控制台分多次返回打印信息，每次都会发送readready并触发read_data
    // 且最后会打印一次空字符，所以不能清空，不然plainTextEdit_Log就是空的。
    // ui->plainTextEdit_Log->clear();

    /* 接收数据 */
    QByteArray bytes = m_SetProcess->readAll();

    QString msg = QString::fromUtf8(bytes.data());
    ui->plainTextEdit_Log->append(msg);
    ui->plainTextEdit_Log->update();
}

void ValSetForm::finished_process()
{
    /* 接收数据 */
    /* 信息输出 */
}

void ValSetForm::error_process()
{
    /* 接收数据 */
    int err_code = m_SetProcess->exitCode();
    QString err = m_SetProcess->errorString();

    /* 显示 */
    ui->plainTextEdit_Log->append(QString("error coed:%1").arg(err_code));
    ui->plainTextEdit_Log->append(err);
    ui->plainTextEdit_Log->update();
}

void ValSetForm::on_listWidget_itemSelectionChanged()
{
    m_Getfilename = ui->listWidget->currentItem()->text();
    m_GetImage.load(m_Getfilename);
    m_YtRoiLabelSet.toClearData();
    m_YtRoiLabelSetJson.toClearData();
    ui->ytRoiShowDisp->toSetImage(m_GetImage);
    ui->ytRoiShowDispJson->toSetImage(m_GetImage);
    m_OverPlayShow.toClearData();
    m_JsonOverPlayShow.toClearData();
    //
    int getindex = m_Getfilename.lastIndexOf(".");
    if (getindex < 0)
    {
        return;
    }

    QFile tsetfile;
    tsetfile.setFileName(m_Getfilename.left(getindex) + ".txt");
    if (tsetfile.exists())
    {
        switch (m_DataSetForm->toGetRunIndex())
        {
        case 0:
        {
            // 矩形目标识别
            m_YtRoiLabelSet.toLoadDetcTxt(m_Getfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);
            m_YtRoiLabelSet.toGenDetcOveplay(m_OverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
            ui->ytRoiShowDisp->toUpdateShow();
            break;
        }
        case 1:
        {
            // OBB识别
            m_YtRoiLabelSet.toLoadObbTxt(m_Getfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);
            m_YtRoiLabelSet.toGenObbOveplay(m_OverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
            ui->ytRoiShowDisp->toUpdateShow();
            break;
        }
        case 2:
        {
            // 语义分割
            m_YtRoiLabelSet.toLoadSegTxt(m_Getfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);
            m_YtRoiLabelSet.toGenSegOveplay(m_OverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
            ui->ytRoiShowDisp->toUpdateShow();
            break;
        }
        case 3:
        {
            // 分类识别
            m_YtRoiLabelSet.toLoadClassTxt(m_Getfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);
            m_YtRoiLabelSet.toGenClassOveplay(m_OverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
            ui->ytRoiShowDisp->toUpdateShow();
            break;
        }
        case 4:
        {
            // 文字识别
            QString getstr = m_YtRoiLabelSet.toLoadOcrTxt(m_Getfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);
            m_OverPlayShow.toClearData();
            if (!getstr.isEmpty())
            {
                m_OverPlayShow.append(
                    DispTxt(getstr, CMvPoint(0, m_GetImage.height()), QFont("Times", 33, 33), Qt::darkGreen));
            }
            ui->ytRoiShowDisp->toUpdateShow();
            break;
        }
        case 5:
        {
            // 语义分割
            m_YtRoiLabelSet.toLoadFourPosTxt(m_Getfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);
            m_YtRoiLabelSet.toGenSegOveplay(m_OverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
            ui->ytRoiShowDisp->toUpdateShow();
            break;
        }
        default:
            break;
        }
    }

    tsetfile.setFileName(m_Getfilename.left(getindex) + ".json");
    if (tsetfile.exists())
    {
        m_YtRoiLabelSetJson.toLoadJsonFile(tsetfile.fileName());
        switch (m_DataSetForm->toGetRunIndex())
        {
        case 0:
        {
            // 矩形目标识别
            m_YtRoiLabelSetJson.toGenDetcOveplay(m_JsonOverPlayShow,
                                                 m_YtYoloSetPro.m_NameList,
                                                 m_YtYoloSetPro.m_ClorDefine);
            ui->ytRoiShowDispJson->toUpdateShow();
            break;
        }
        case 1:
        {
            // OBB识别
            m_YtRoiLabelSetJson.toGenObbOveplay(m_JsonOverPlayShow,
                                                m_YtYoloSetPro.m_NameList,
                                                m_YtYoloSetPro.m_ClorDefine);
            ui->ytRoiShowDispJson->toUpdateShow();
            break;
        }
        case 2:
        {
            // 语义分割
            m_YtRoiLabelSetJson.toGenSegOveplay(m_JsonOverPlayShow,
                                                m_YtYoloSetPro.m_NameList,
                                                m_YtYoloSetPro.m_ClorDefine);
            ui->ytRoiShowDispJson->toUpdateShow();
            break;
        }
        case 3:
        {
            // 分类识别
            m_YtRoiLabelSetJson.toGenClassOveplay(m_JsonOverPlayShow,
                                                  m_YtYoloSetPro.m_NameList,
                                                  m_YtYoloSetPro.m_ClorDefine);
            ui->ytRoiShowDispJson->toUpdateShow();
            break;
        }
        case 4:
        {
            // 文字识别
            // m_YtRoiLabelSetJson.toGenClassOveplay(m_JsonOverPlayShow,m_YtYoloSetPro.m_NameList,m_YtYoloSetPro.m_ClorDefine);

            m_JsonOverPlayShow.toClearData();
            if (m_YtRoiLabelSetJson.m_SetLabeset.size() > 0)
            {
                m_JsonOverPlayShow.append(DispTxt(m_YtRoiLabelSetJson.m_SetLabeset[0].Name,
                                                  CMvPoint(0, m_YtRoiLabelSetJson.m_imHight),
                                                  QFont("Times", 33, 33),
                                                  Qt::darkGreen));
            }
            ui->ytRoiShowDispJson->toUpdateShow();
            break;
        }
        case 5:
        {
            // 语义分割
            m_YtRoiLabelSetJson.toGenSegOveplay(m_JsonOverPlayShow,
                                                m_YtYoloSetPro.m_NameList,
                                                m_YtYoloSetPro.m_ClorDefine);
            ui->ytRoiShowDispJson->toUpdateShow();
            break;
        }
        default:
            break;
        }
    }
}

void ValSetForm::on_PB_ProVal_clicked()
{
    if (!QFile::exists(YtYoloDefine::toGetPythonPath() + "/python.exe"))
    {
        QMessageBox::warning(0, u8"警告", u8"运行路径异常", u8"确定");
        return;
    }

    QDir temdir;
    temdir.setPath(YtYoloDefine::toGetValuePath() + "/" + m_ProcessName);
    temdir.mkpath(YtYoloDefine::toGetValuePath() + "/" + m_ProcessName);

    if (m_SetProcess == nullptr)
    {
        m_SetProcess = new QProcess(this); // 读命令行标准数据（兼容）
        //
        connect(m_SetProcess, SIGNAL(readyRead()), this, SLOT(read_data()));               // 读命令行数据
        connect(m_SetProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(read_data())); // 读命令行标准数据（兼容）
        connect(m_SetProcess,
                SIGNAL(errorOccurred(QProcess::ProcessError)),
                this,
                SLOT(error_process()));                                               // 命令行错误处理
        connect(m_SetProcess, SIGNAL(finished(int)), this, SLOT(finished_process())); // 命令行结束处理

        m_SetProcess->setProcessChannelMode(QProcess::MergedChannels);
        m_SetProcess->start("cmd.exe");
    }

    // D:\PythonEnv\python.exe  D:\PythonEnv\Scripts\yolo.exe settings runs_dir=D:/PythonEnv/runTest mlflow=false
    // wandb=false datasets_dir=D:/PythonEnv/runTest/datasets

    // D:\PythonEnv\python.exe  D:\PythonEnv\Scripts\yolo.exe detect train
    // data=G:/YoloV8ProDo/DataSheet/目标识别/Config.yaml model=D:/PythonEnv/model/yolo11s.pt epochs=30 imgsz=640
    // name=train exist_ok=true

    QString cmdDir = YtYoloDefine::toGetPythonPath().left(1) + ":" + "\r\n";
    m_SetProcess->write(cmdDir.toLocal8Bit().data(), qint64(cmdDir.toLocal8Bit().size()));
    //

    cmdDir = YtYoloDefine::toGetPythonPath().left(1) + ":" + "\r\n";
    m_SetProcess->write(cmdDir.toLocal8Bit().data(), qint64(cmdDir.toLocal8Bit().size()));

    if (m_DataSetForm->toGetRunIndex() == 4)
    {
        QFile temCsv;
        temCsv.setFileName(YtYoloDefine::toGetValuePath() + "/" + m_ProcessName + "/ProFile.csv");
        qDebug() << temCsv.fileName() << "ppp";

        if (!temCsv.exists())
        {
            return;
        }
        cmdDir = QString("cd %1/OCRScrept \r\n").arg(YtYoloDefine::toGetPythonPath());
        m_SetProcess->write(cmdDir.toLocal8Bit().data(), qint64(cmdDir.toLocal8Bit().size()));

        //    //执行运行目录设置
        cmdDir = QString("%1/python.exe predict.py --cfg %2/OWN_config.yaml --checkpoint %3 --image_dir %4\r\n")
                     .arg(YtYoloDefine::toGetPythonPath())
                     .arg(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName)
                     .arg(YtYoloDefine::toGetTrainPath() + "/" + m_ProcessName + "/model_best.pth")
                     .arg(temCsv.fileName());

        m_SetProcess->write(cmdDir.toLocal8Bit().data(), qint64(cmdDir.toLocal8Bit().size()));

        return;
    }
    if (m_DataSetForm->toGetRunIndex() == 5)
    {
        QFile temCsv;
        temCsv.setFileName(YtYoloDefine::toGetValuePath() + "/" + m_ProcessName + "/ProFile.csv");
        qDebug() << temCsv.fileName() << "ppp";

        if (!temCsv.exists())
        {
            return;
        }
        cmdDir = QString("cd %1/DBNet \r\n").arg(YtYoloDefine::toGetPythonPath());
        m_SetProcess->write(cmdDir.toLocal8Bit().data(), qint64(cmdDir.toLocal8Bit().size()));

        //    //执行运行目录设置
        cmdDir = QString("%1/python.exe predict.py --model_path %2  --input_folder %3 --thre %4\r\n")
                     .arg(YtYoloDefine::toGetPythonPath())
                     .arg(YtYoloDefine::toGetTrainPath() + "/" + m_ProcessName + "/weights/model_best_recall.pth")
                     .arg(temCsv.fileName())
                     .arg(ui->doubleSpinBox->value());

        m_SetProcess->write(cmdDir.toLocal8Bit().data(), qint64(cmdDir.toLocal8Bit().size()));

        return;
    }
    cmdDir = QString("%1/python.exe %2/Scripts/yolo.exe settings runs_dir=%3 mlflow=false wandb=false\r\n")
                 .arg(YtYoloDefine::toGetPythonPath())
                 .arg(YtYoloDefine::toGetPythonPath())
                 .arg(temdir.path().replace("/", "\\\\"));

    //    qDebug()<<"Local8bit"<<cmdDir.toLocal8Bit().toHex(' ')<<cmdDir.toLocal8Bit().size();
    //    qDebug()<<"lantinbit"<<cmdDir.toLatin1().toHex(' ')<<cmdDir.toLatin1().size();

    m_SetProcess->write(cmdDir.toLocal8Bit().data(), qint64(cmdDir.toLocal8Bit().size()));

    //
    QStringList protask;
    QFile temsetfile;
    protask << "detect" << "obb" << "segment" << "classify";
    temsetfile.setFileName(QString("%1/%2/%3/train/weights/best.pt")
                               .arg(YtYoloDefine::toGetTrainPath())
                               .arg(m_ProcessName)
                               .arg(protask.at(m_DataSetForm->toGetRunIndex())));
    qDebug() << temsetfile.fileName() << "ppp";
    if (!temsetfile.exists())
    {
        return;
    }
    QFile temCsv;
    temCsv.setFileName(YtYoloDefine::toGetValuePath() + "/" + m_ProcessName + "/ProFile.csv");
    qDebug() << temCsv.fileName() << "ppp";

    if (!temCsv.exists())
    {
        return;
    }
    // 清除之前的推理结果
    //     QFile OpenFile(temCsv);
    QFile OpenFile(temCsv.fileName());
    if (!OpenFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    QTextStream in(&OpenFile);
    while (!in.atEnd())
    {
        QString line = in.readLine();
        QString tName = line.mid(0, line.lastIndexOf(".")) + ".txt";
        qDebug() << "delelte file: " << tName;
        QFile(tName).remove();
    }
    OpenFile.close();

    // D:\PythonEnv\python.exe  D:\PythonEnv\Scripts\yolo.exe predict model=D:\PythonEnv\model\yolo11n-seg.pt
    // source=G:\YoloV8ProDo\Valsheet\11 save=False show=false conf=0.1 save_txt=true exist_ok=true
    cmdDir = QString("%1/python.exe %2/Scripts/yolo.exe predict model=%3 source=%4 save=False show=false conf=%5 "
                     "iou=%6 save_txt=true save_conf=true exist_ok=true max_det=1000\r\n")
                 .arg(YtYoloDefine::toGetPythonPath()) // 1 运行目录
                 .arg(YtYoloDefine::toGetPythonPath()) // 2 运行目录
                 .arg(temsetfile.fileName())           // 3 模型位置
                 .arg(temCsv.fileName())               // 4 csv文件位置;
                 .arg(ui->doubleSpinBox->value())      // 5 config
                 .arg(ui->dSpinBox_IOU->value());      // 6 iou_nms
    qDebug() << cmdDir << "PPP";
    m_SetProcess->write(cmdDir.toLocal8Bit().data(), qint64(cmdDir.toLocal8Bit().size()));
}

void ValSetForm::on_PB_RunReplace_clicked()
{

    m_YtRoiLabelSet.toSaveJson(QFileInfo(m_Getfilename).path());
}

void ValSetForm::on_PB_OverJson_clicked()
{
    YtRoiLabelSet tYtRoiLabelSet; // 标签数据

    tYtRoiLabelSet.toClearData();
    for (int i = 0; i < ui->listWidget->count(); i++)
    {
        ui->listWidget->setCurrentRow(i);
        YtSleeP(100);
        QString tGetfilename = ui->listWidget->item(i)->text();

        //
        int getindex = tGetfilename.lastIndexOf(".");
        if (getindex < 0)
        {
            continue;
        }
        QFile tsetfile;
        tsetfile.setFileName(tGetfilename.left(getindex) + ".txt");
        if (tsetfile.exists())
        {
            switch (m_DataSetForm->toGetRunIndex())
            {
            case 0:
            {
                // 矩形目标识别
                tYtRoiLabelSet.toLoadDetcTxt(tGetfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);

                break;
            }
            case 1:
            {
                // OBB识别
                tYtRoiLabelSet.toLoadObbTxt(tGetfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);

                break;
            }
            case 2:
            {
                // 语义分割
                tYtRoiLabelSet.toLoadSegTxt(tGetfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);

                break;
            }
            case 3:
            {
                // 分类识别
                tYtRoiLabelSet.toLoadClassTxt(tGetfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);

                break;
            }
            case 4:
            {
                // 字符识别
                tYtRoiLabelSet.toLoadOcrTxt(tGetfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);

                break;
            }
            case 5:
            {
                // 字符目标识别
                tYtRoiLabelSet.toLoadFourPosTxt(tGetfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);

                break;
            }
            default:
                break;
            }
            tYtRoiLabelSet.toSaveJson(QFileInfo(tGetfilename).path());
        }
    }

    QMessageBox::about(0, u8"提示", u8"标签转换完成");
}

void ValSetForm::on_PB_RunTset_clicked()
{
    m_GetSavePath = QFileDialog::getExistingDirectory();
    if (m_GetSavePath.isEmpty())
    {
        return;
    }
    QThreadPool::globalInstance()->start(
        [this]()
        {
            toDoRunPP();
        });
}

void ValSetForm::on_PB_RunOCR_clicked()
{
    // 只切割确认的json位置
    QString getpath = QFileDialog::getExistingDirectory();
    if (getpath.isEmpty())
    {
        return;
    }
    QThreadPool::globalInstance()->start(
        [this, getpath]()
        {
            toDoRunOCR(getpath);
        });
}
