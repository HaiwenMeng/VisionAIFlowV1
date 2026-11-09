#pragma once

#include <QWidget>

class QJsonObject;

namespace Ui
{
class ClassificationLabelForm;
}

class ClassificationLabelForm final : public QWidget
{
    Q_OBJECT

public:
    explicit ClassificationLabelForm(QWidget *parent = nullptr);
    ~ClassificationLabelForm() override;
    void initialize(const QString &taskName);
    static bool readImageLabel(const QJsonObject &object, QString *label, QString *errorMessage);

private:
    void loadImages();
    void showCurrentImage();
    void assignSelected(const QString &label);
    void reportError(const QString &title, const QString &errorMessage);
    QString currentDirectory() const;

    Ui::ClassificationLabelForm *ui;
    QString m_taskName;
};
