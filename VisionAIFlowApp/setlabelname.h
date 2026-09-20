#ifndef SETLABELNAME_H
#define SETLABELNAME_H

#include <QColor>
#include <QDialog>
#include <QString>

namespace Ui
{
class SetLabelName;
}

class SetLabelName : public QDialog
{
    Q_OBJECT

public:
    explicit SetLabelName(QWidget *parent = nullptr);
    ~SetLabelName();

private slots:
    void on_PB_Confirm_clicked();
    void on_PB_SetColor_clicked();
    void on_PB_Modify_clicked();
    void on_PB_Cancle_clicked();

public:
    QColor m_SetColor;
    QString m_SetName;

public:
    void toSetData(QString NameS, QColor SetColor);
    void toSetModifyData(const QString &name, const QColor &color, int labelIndex, const QString &projectPath);
    void toGetData(QString &NameS, QColor &SetColor);
    bool toModifyLabelName(const QString &newName, QString *errorMessage);

private:
    Ui::SetLabelName *ui;
    QString m_OriginalName;
    QString m_ProjectPath;
    int m_LabelIndex = -1;
};

#endif // SETLABELNAME_H
