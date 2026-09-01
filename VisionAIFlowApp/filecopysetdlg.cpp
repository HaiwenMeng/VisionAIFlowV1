#include "filecopysetdlg.h"
#include "ui_filecopysetdlg.h"
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QDirIterator>
#include <QMessageBox>
#include <QTextStream>
#include <cstring>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "ytvisiondefine.h"

namespace
{
bool LoadYtImage(QImage &outImage, const QString &fileName, QString &errorMessage)
{
    QFile dataFile(fileName);
    if (!dataFile.exists())
    {
        errorMessage = QString(u8"图像文件不存在: %1").arg(fileName);
        return false;
    }

    if (!dataFile.open(QIODevice::ReadOnly))
    {
        errorMessage = QString(u8"无法打开图像文件: %1").arg(fileName);
        return false;
    }

    QDataStream stream(&dataFile);
    QByteArray encodedSlice;
    int imageWidth = 0;
    int imageHeight = 0;
    int imageType = 0;
    int channelCount = 0;
    QVector<int> sliceOffsets;
    stream >> imageWidth >> imageHeight >> imageType >> sliceOffsets >> channelCount;
    if (stream.status() != QDataStream::Ok || imageWidth <= 0 || imageHeight <= 0 ||
        (channelCount != 1 && channelCount != 3))
    {
        errorMessage = QString(u8"图像文件格式无效: %1").arg(fileName);
        return false;
    }

    const QImage::Format imageFormat = channelCount == 1 ? QImage::Format_Grayscale8 : QImage::Format_RGB888;
    outImage = QImage(imageWidth, imageHeight, imageFormat);
    if (outImage.isNull())
    {
        errorMessage = QString(u8"无法分配图像内存: %1").arg(fileName);
        return false;
    }

    for (int sliceOffset : sliceOffsets)
    {
        stream >> encodedSlice;
        if (stream.status() != QDataStream::Ok)
        {
            errorMessage = QString(u8"图像文件数据不完整: %1").arg(fileName);
            return false;
        }

        QImage slice;
        if (!slice.loadFromData(encodedSlice, "jpg"))
        {
            errorMessage = QString(u8"图像切片解码失败: %1").arg(fileName);
            return false;
        }

        const QImage convertedSlice = slice.convertToFormat(imageFormat);
        const qsizetype byteOffset = static_cast<qsizetype>(sliceOffset) * outImage.bytesPerLine();
        if (sliceOffset < 0 || byteOffset + convertedSlice.sizeInBytes() > outImage.sizeInBytes())
        {
            errorMessage = QString(u8"图像切片位置无效: %1").arg(fileName);
            return false;
        }

        std::memcpy(outImage.bits() + byteOffset,
                    convertedSlice.constBits(),
                    static_cast<size_t>(convertedSlice.sizeInBytes()));
    }

    return true;
}

bool LoadImage(QImage &outImage, const QString &fileName, QString &errorMessage)
{
    if (fileName.endsWith(QStringLiteral(".ytimage"), Qt::CaseInsensitive))
    {
        return LoadYtImage(outImage, fileName, errorMessage);
    }

    const cv::Mat source = cv::imread(fileName.toStdString(), cv::IMREAD_UNCHANGED);
    if (source.empty())
    {
        errorMessage = QString(u8"无法读取图像文件: %1").arg(fileName);
        return false;
    }

    cv::Mat converted;
    QImage::Format imageFormat = QImage::Format_Invalid;
    if (source.channels() == 1)
    {
        converted = source;
        imageFormat = QImage::Format_Grayscale8;
    }
    else if (source.channels() == 3)
    {
        cv::cvtColor(source, converted, cv::COLOR_BGR2RGB);
        imageFormat = QImage::Format_RGB888;
    }
    else if (source.channels() == 4)
    {
        cv::cvtColor(source, converted, cv::COLOR_BGRA2RGBA);
        imageFormat = QImage::Format_RGBA8888;
    }
    else
    {
        errorMessage = QString(u8"不支持的图像通道数: %1").arg(fileName);
        return false;
    }

    outImage =
        QImage(converted.data, converted.cols, converted.rows, static_cast<qsizetype>(converted.step), imageFormat)
            .copy();
    if (outImage.isNull())
    {
        errorMessage = QString(u8"图像转换失败: %1").arg(fileName);
        return false;
    }

    return true;
}
} // namespace

FileCopySetDlg::FileCopySetDlg(QWidget *parent) : QDialog(parent), ui(new Ui::FileCopySetDlg)
{
    ui->setupUi(this);
}

FileCopySetDlg::~FileCopySetDlg()
{
    delete ui;
}

void FileCopySetDlg::on_PB_ClearImlist_clicked()
{
    ui->LW_FileList->clear();
}

void FileCopySetDlg::on_PB_AddFileTxt_clicked()
{
    QString FileName = QFileDialog::getOpenFileName(this, u8"选择图像信息文件", "*.txt");

    QFile OpenFile(FileName);
    if (!OpenFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    QTextStream in(&OpenFile);
    while (!in.atEnd())
    {
        QString line = in.readLine();
        ui->LW_FileList->addItem(line);
    }

    OpenFile.close();
}

void FileCopySetDlg::on_PB_AddFileList_clicked()
{
    QStringList temlis =
        QFileDialog::getOpenFileNames(this, u8"选择图象文件", "", u8"图像文件( *.bmp *.jpg *.png *.jpeg)");
    if (temlis.size() < 1)
    {
        return;
    }
    ui->LW_FileList->addItems(temlis);
}

void FileCopySetDlg::on_PB_AddFoldImList_clicked()
{
    QString FilePath = QFileDialog::getExistingDirectory();
    if (FilePath.isEmpty())
    {
        return;
    }
    const QStringList imageFilters = {QStringLiteral("*.tif"),
                                      QStringLiteral("*.tiff"),
                                      QStringLiteral("*.gif"),
                                      QStringLiteral("*.bmp"),
                                      QStringLiteral("*.jpg"),
                                      QStringLiteral("*.jpeg"),
                                      QStringLiteral("*.jp2"),
                                      QStringLiteral("*.png"),
                                      QStringLiteral("*.pcx"),
                                      QStringLiteral("*.pgm"),
                                      QStringLiteral("*.ppm"),
                                      QStringLiteral("*.pbm"),
                                      QStringLiteral("*.xwd"),
                                      QStringLiteral("*.ima"),
                                      QStringLiteral("*.ytimage")};
    const QDirIterator::IteratorFlags iteratorFlags =
        ui->CB_PathRestv->isChecked() ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator iterator(FilePath, imageFilters, QDir::Files, iteratorFlags);
    while (iterator.hasNext())
    {
        ui->LW_FileList->addItem(iterator.next());
    }
}

void FileCopySetDlg::on_PB_Confirm_clicked()
{
    accept();
}

void FileCopySetDlg::on_PB_Cancle_clicked()
{
    reject();
}

void FileCopySetDlg::on_LW_FileList_itemSelectionChanged()
{
    QString errorMessage;
    if (!LoadImage(m_GetImage, ui->LW_FileList->currentItem()->text(), errorMessage))
    {
        qWarning() << errorMessage;
        QMessageBox::critical(this, QString(u8"图像读取失败"), errorMessage);
        return;
    }
    if (!ui->baseAnnoDisplayWidget->setImage(m_GetImage, &errorMessage))
    {
        QMessageBox::critical(this, QString(u8"图像显示失败"), errorMessage);
    }
}

bool FileCopySetDlg::toGetCopyImage(QString DestPath)
{
    int count = ui->LW_FileList->count();
    QDir temdir;
    temdir.mkpath(DestPath);
    if (count < 1)
    {
        return false;
    }
    QString OldName, NewName;

    for (int i = 0; i < count; i++)
    {
        OldName = ui->LW_FileList->item(i)->text().replace("\\", "/");
        NewName = DestPath + "/" + OldName.split("/").last();
        qInfo() << QString(u8"拷贝文件") << OldName;
        qInfo() << QString(u8"目标位置") << i << NewName;

        bool iscopy = QFile::copy(OldName, NewName);
        qDebug() << i << iscopy << OldName << NewName;
    }

    return true;
}

QString FileCopySetDlg::toGetTitleName()
{
    return ui->LE_Titile->text();
}

void FileCopySetDlg::toSetTitleName(QString StetitleName)
{
    ui->LE_Titile->setText(StetitleName);
    // ui->LE_Titile->setReadOnly(true);
}

QStringList FileCopySetDlg::toGetFileList()
{
    QStringList getlis;
    for (int i = 0; i < ui->LW_FileList->count(); i++)
    {
        getlis << ui->LW_FileList->item(i)->text();
    }
    return getlis;
}
