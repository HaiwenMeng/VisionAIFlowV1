#pragma once

#include "visionaiflow/foundation/Result.h"

#include <vector>

#if defined(VISIONAIFLOW_ANNOTATION_LIBRARY)
#define VISIONAIFLOW_ANNOTATION_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_ANNOTATION_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::annotation
{
struct Point final
{
    double x{0.0};
    double y{0.0};
};

struct Rect final
{
    double x{0.0};
    double y{0.0};
    double width{0.0};
    double height{0.0};
};

struct LineSegment final
{
    Point first;
    Point second;
};

struct ImageSize final
{
    int width{0};
    int height{0};
};

struct ViewportTransform final
{
    double zoom{1.0};
    Point pan;
};

VISIONAIFLOW_ANNOTATION_EXPORT foundation::Result<void> ValidatePoint(const Point &point);
VISIONAIFLOW_ANNOTATION_EXPORT foundation::Result<void> ValidateRect(const Rect &rect);
VISIONAIFLOW_ANNOTATION_EXPORT foundation::Result<void> ValidatePolygon(const std::vector<Point> &polygon);
VISIONAIFLOW_ANNOTATION_EXPORT foundation::Result<LineSegment> CanonicalizeLine(const LineSegment &line);
VISIONAIFLOW_ANNOTATION_EXPORT bool IsSameUndirectedLine(const LineSegment &left, const LineSegment &right, double tolerance);
VISIONAIFLOW_ANNOTATION_EXPORT double SignedPolygonArea(const std::vector<Point> &polygon);
VISIONAIFLOW_ANNOTATION_EXPORT foundation::Result<void> ValidateImageSize(const ImageSize &imageSize);
VISIONAIFLOW_ANNOTATION_EXPORT foundation::Result<void> ValidateRectInsideImage(const Rect &rect, const ImageSize &imageSize);
VISIONAIFLOW_ANNOTATION_EXPORT foundation::Result<void> ValidateLineInsideImage(const LineSegment &line, const ImageSize &imageSize);
VISIONAIFLOW_ANNOTATION_EXPORT foundation::Result<void> ValidateViewportTransform(const ViewportTransform &transform);
VISIONAIFLOW_ANNOTATION_EXPORT foundation::Result<Point> ViewportToImage(const Point &viewportPoint, const ViewportTransform &transform);
VISIONAIFLOW_ANNOTATION_EXPORT foundation::Result<Point> ImageToViewport(const Point &imagePoint, const ViewportTransform &transform);
VISIONAIFLOW_ANNOTATION_EXPORT foundation::Result<Point> ClampPointToImage(const Point &imagePoint, const ImageSize &imageSize);
VISIONAIFLOW_ANNOTATION_EXPORT foundation::Result<Rect> ClampRectToImage(const Rect &rect, const ImageSize &imageSize);
}
