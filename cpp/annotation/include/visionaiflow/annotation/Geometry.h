#pragma once

#include "visionaiflow/foundation/Result.h"

#include <vector>

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

foundation::Result<void> ValidatePoint(const Point &point);
foundation::Result<void> ValidateRect(const Rect &rect);
foundation::Result<void> ValidatePolygon(const std::vector<Point> &polygon);
foundation::Result<LineSegment> CanonicalizeLine(const LineSegment &line);
bool IsSameUndirectedLine(const LineSegment &left, const LineSegment &right, double tolerance);
double SignedPolygonArea(const std::vector<Point> &polygon);
foundation::Result<void> ValidateImageSize(const ImageSize &imageSize);
foundation::Result<void> ValidateRectInsideImage(const Rect &rect, const ImageSize &imageSize);
foundation::Result<void> ValidateLineInsideImage(const LineSegment &line, const ImageSize &imageSize);
foundation::Result<void> ValidateViewportTransform(const ViewportTransform &transform);
foundation::Result<Point> ViewportToImage(const Point &viewportPoint, const ViewportTransform &transform);
foundation::Result<Point> ImageToViewport(const Point &imagePoint, const ViewportTransform &transform);
foundation::Result<Point> ClampPointToImage(const Point &imagePoint, const ImageSize &imageSize);
foundation::Result<Rect> ClampRectToImage(const Rect &rect, const ImageSize &imageSize);
}
