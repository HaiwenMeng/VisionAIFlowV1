#ifndef FILECOPYSETDLG_H
#define FILECOPYSETDLG_H

#include <QDialog>

namespace Ui {
class FileCopySetDlg;
}

class FileCopySetDlg : public QDialog
{
    Q_OBJECT

public:
    explicit FileCopySetDlg(QWidget *parent = nullptr);
    ~FileCopySetDlg();

private slots:
    void on_PB_ClearImlist_clicked();

    void on_PB_AddFileTxt_clicked();

    void on_PB_AddFileList_clicked();

    void on_PB_AddFoldImList_clicked();

    void on_PB_Confirm_clicked();

    void on_PB_Cancle_clicked();

    void on_LW_FileList_itemSelectionChanged();

    void on_CB_RoiSet_stateChanged(int arg1);

private:
    Ui::FileCopySetDlg *ui;
public:
    QImage m_GetImage;
public:
    bool toGetCopyImage(QString DestPath);
     QString toGetTitleName();
     void toSetTitleName(QString StetitleName);
     QStringList toGetFileList();

};

#endif // FILECOPYSETDLG_H
