#ifndef VISIONAIFLOW_BASEIMAGEANNOTATEWIDGET_H
#define VISIONAIFLOW_BASEIMAGEANNOTATEWIDGET_H

#include "BaseAnnoDisplayWidget.h"
#include "app/AppTypes.h"

class ImageAnnotateWidget final : public BaseAnnoDisplayWidget
{
    Q_OBJECT

public:
    explicit ImageAnnotateWidget(QWidget *parent = nullptr);

    bool loadImage(const QString &imagePath);
    void clearTempResult();
    void setTempResult(const TempInferenceResult &result);
    void setAnnotations(const QList<AnnotationObject> &annotations);
};

#endif // VISIONAIFLOW_BASEIMAGEANNOTATEWIDGET_H
