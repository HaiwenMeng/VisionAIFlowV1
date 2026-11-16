#pragma once

#include <QWidget>

namespace Ui
{
class LineAnnoForm;
}

class LineAnnoForm final : public QWidget
{
    Q_OBJECT

public:
    explicit LineAnnoForm(QWidget *parent = nullptr);
    ~LineAnnoForm() override;
    void initialize(const QString &taskName);

private:
    void importImages();
    void loadImages();
    void showImage();
    void save();
    void reportError(const QString &errorMessage);
    QString imagePath() const;
    QString directoryPath() const;

    Ui::LineAnnoForm *ui;
    QString m_taskName;
    int m_selected = -1;
    bool m_loadedValid = false;
};
