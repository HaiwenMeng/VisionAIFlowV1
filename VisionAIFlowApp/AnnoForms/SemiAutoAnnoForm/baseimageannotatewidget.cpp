#include "baseimageannotatewidget.h"

#include <QDebug>

ImageAnnotateWidget::ImageAnnotateWidget(QWidget *parent)
    : BaseAnnoDisplayWidget(parent)
{
}

bool ImageAnnotateWidget::loadImage(const QString &imagePath)
{
    QString errorMessage;
    if (BaseAnnoDisplayWidget::loadImage(imagePath, &errorMessage))
    {
        return true;
    }

    qCritical().noquote() << "加载辅助标注图像失败:" << errorMessage;
    return false;
}

void ImageAnnotateWidget::clearTempResult()
{
    clearTempPreview();
}

void ImageAnnotateWidget::setTempResult(const TempInferenceResult &result)
{
    BaseAnnoTempPreview preview;
    preview.valid = result.valid;
    preview.clickPointImage = result.clickPointImage;
    preview.promptRectImage = result.promptRectImage;
    preview.hasClick = result.hasClick;
    preview.hasRect = result.hasRect;
    preview.contourImage = result.contourImage;
    preview.minAreaRectImage = result.minAreaRectImage;
    setTempPreview(preview);
}

void ImageAnnotateWidget::setAnnotations(const QList<AnnotationObject> &annotations)
{
    BaseAnnoAnnotationList converted;
    converted.reserve(annotations.size());
    for (const AnnotationObject &annotation : annotations)
    {
        BaseAnnoAnnotation item;
        item.shapeIndex = annotation.shapeIndex;
        item.label = annotation.label;
        item.colorValue = annotation.colorValue;
        item.shapeType = annotation.shapeType == AnnotationShapeType::Polygon
            ? BaseAnnoShapeType::Polygon
            : BaseAnnoShapeType::Rectangle;
        item.pointsImage = annotation.shapeType == AnnotationShapeType::Polygon
            ? annotation.polygonImage
            : annotation.rectPolygonImage;
        converted.append(item);
    }
    BaseAnnoDisplayWidget::setAnnotations(converted);
}
