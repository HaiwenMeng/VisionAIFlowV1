#include "visionaiflow/annotation/Geometry.h"

#include <algorithm>
#include <cmath>

namespace visionaiflow::annotation
{
namespace
{
constexpr double MinimumArea = 1.0e-12;
constexpr double MinimumLineLength = 1.0e-6;

bool IsFinite(const Point &point)
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

double Cross(const Point &origin, const Point &left, const Point &right)
{
    return (left.x - origin.x) * (right.y - origin.y) - (left.y - origin.y) * (right.x - origin.x);
}

bool IsOnSegment(const Point &start, const Point &end, const Point &point)
{
    return point.x >= std::min(start.x, end.x) && point.x <= std::max(start.x, end.x) && point.y >= std::min(start.y, end.y) && point.y <= std::max(start.y, end.y);
}

bool SegmentsIntersect(const Point &a, const Point &b, const Point &c, const Point &d)
{
    const double first = Cross(a, b, c);
    const double second = Cross(a, b, d);
    const double third = Cross(c, d, a);
    const double fourth = Cross(c, d, b);
    if (((first > 0.0 && second < 0.0) || (first < 0.0 && second > 0.0)) && ((third > 0.0 && fourth < 0.0) || (third < 0.0 && fourth > 0.0))) return true;
    return (first == 0.0 && IsOnSegment(a, b, c)) || (second == 0.0 && IsOnSegment(a, b, d)) || (third == 0.0 && IsOnSegment(c, d, a)) || (fourth == 0.0 && IsOnSegment(c, d, b));
}

bool IsLexicographicallyBefore(const Point &left, const Point &right)
{
    return left.x < right.x || (left.x == right.x && left.y < right.y);
}

double SquaredDistance(const Point &left, const Point &right)
{
    const double dx = left.x - right.x;
    const double dy = left.y - right.y;
    return dx * dx + dy * dy;
}

bool IsPointInsideImage(const Point &point, const ImageSize &imageSize)
{
    return point.x >= 0.0 && point.y >= 0.0 && point.x < static_cast<double>(imageSize.width) && point.y < static_cast<double>(imageSize.height);
}
}

foundation::Result<void> ValidatePoint(const Point &point)
{
    if (!IsFinite(point)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Point coordinates must be finite"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidateRect(const Rect &rect)
{
    if (!std::isfinite(rect.x) || !std::isfinite(rect.y) || !std::isfinite(rect.width) || !std::isfinite(rect.height)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Rectangle coordinates must be finite"));
    if (rect.width <= 0.0 || rect.height <= 0.0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Rectangle width and height must be positive"));
    return foundation::Result<void>::Success();
}

double SignedPolygonArea(const std::vector<Point> &polygon)
{
    if (polygon.size() < 3U) return 0.0;
    double twiceArea = 0.0;
    for (size_t index = 0; index < polygon.size(); ++index)
    {
        const Point &current = polygon[index];
        const Point &next = polygon[(index + 1U) % polygon.size()];
        twiceArea += current.x * next.y - next.x * current.y;
    }
    return twiceArea * 0.5;
}

foundation::Result<void> ValidatePolygon(const std::vector<Point> &polygon)
{
    if (polygon.size() < 3U) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Polygon requires at least three points"));
    for (const Point &point : polygon)
    {
        const auto validation = ValidatePoint(point);
        if (!validation.IsSuccess()) return validation;
    }
    if (std::abs(SignedPolygonArea(polygon)) <= MinimumArea) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Polygon area must be non-zero"));
    for (size_t first = 0; first < polygon.size(); ++first)
    {
        const size_t firstNext = (first + 1U) % polygon.size();
        for (size_t second = first + 1U; second < polygon.size(); ++second)
        {
            const size_t secondNext = (second + 1U) % polygon.size();
            if (first == second || firstNext == second || secondNext == first) continue;
            if (SegmentsIntersect(polygon[first], polygon[firstNext], polygon[second], polygon[secondNext])) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Polygon must not self-intersect"));
        }
    }
    return foundation::Result<void>::Success();
}

foundation::Result<LineSegment> CanonicalizeLine(const LineSegment &line)
{
    const auto first = ValidatePoint(line.first);
    if (!first.IsSuccess()) return foundation::Result<LineSegment>::Failure(first.Failure());
    const auto second = ValidatePoint(line.second);
    if (!second.IsSuccess()) return foundation::Result<LineSegment>::Failure(second.Failure());
    if (SquaredDistance(line.first, line.second) <= MinimumLineLength * MinimumLineLength) return foundation::Result<LineSegment>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Line segment length is below the minimum valid length"));
    return foundation::Result<LineSegment>::Success(IsLexicographicallyBefore(line.second, line.first) ? LineSegment{line.second, line.first} : line);
}

bool IsSameUndirectedLine(const LineSegment &left, const LineSegment &right, const double tolerance)
{
    if (tolerance < 0.0 || !std::isfinite(tolerance)) return false;
    const auto normalizedLeft = CanonicalizeLine(left);
    const auto normalizedRight = CanonicalizeLine(right);
    if (!normalizedLeft.IsSuccess() || !normalizedRight.IsSuccess()) return false;
    const auto samePoint = [tolerance](const Point &first, const Point &second) { return std::abs(first.x - second.x) <= tolerance && std::abs(first.y - second.y) <= tolerance; };
    return samePoint(normalizedLeft.Value().first, normalizedRight.Value().first) && samePoint(normalizedLeft.Value().second, normalizedRight.Value().second);
}

foundation::Result<void> ValidateImageSize(const ImageSize &imageSize)
{
    if (imageSize.width <= 0 || imageSize.height <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Image dimensions must be positive"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidateRectInsideImage(const Rect &rect, const ImageSize &imageSize)
{
    const auto rectValidation = ValidateRect(rect);
    if (!rectValidation.IsSuccess()) return rectValidation;
    const auto sizeValidation = ValidateImageSize(imageSize);
    if (!sizeValidation.IsSuccess()) return sizeValidation;
    if (rect.x < 0.0 || rect.y < 0.0 || rect.x + rect.width > static_cast<double>(imageSize.width) || rect.y + rect.height > static_cast<double>(imageSize.height)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Rectangle must stay inside image half-open bounds"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidateLineInsideImage(const LineSegment &line, const ImageSize &imageSize)
{
    const auto canonical = CanonicalizeLine(line);
    if (!canonical.IsSuccess()) return foundation::Result<void>::Failure(canonical.Failure());
    const auto sizeValidation = ValidateImageSize(imageSize);
    if (!sizeValidation.IsSuccess()) return sizeValidation;
    if (!IsPointInsideImage(canonical.Value().first, imageSize) || !IsPointInsideImage(canonical.Value().second, imageSize)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Line endpoints must stay inside image half-open bounds"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidateViewportTransform(const ViewportTransform &transform)
{
    if (!std::isfinite(transform.zoom) || transform.zoom <= 0.0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Viewport zoom must be finite and positive"));
    return ValidatePoint(transform.pan);
}

foundation::Result<Point> ViewportToImage(const Point &viewportPoint, const ViewportTransform &transform)
{
    const auto pointValidation = ValidatePoint(viewportPoint);
    if (!pointValidation.IsSuccess()) return foundation::Result<Point>::Failure(pointValidation.Failure());
    const auto transformValidation = ValidateViewportTransform(transform);
    if (!transformValidation.IsSuccess()) return foundation::Result<Point>::Failure(transformValidation.Failure());
    return foundation::Result<Point>::Success({(viewportPoint.x - transform.pan.x) / transform.zoom, (viewportPoint.y - transform.pan.y) / transform.zoom});
}

foundation::Result<Point> ImageToViewport(const Point &imagePoint, const ViewportTransform &transform)
{
    const auto pointValidation = ValidatePoint(imagePoint);
    if (!pointValidation.IsSuccess()) return foundation::Result<Point>::Failure(pointValidation.Failure());
    const auto transformValidation = ValidateViewportTransform(transform);
    if (!transformValidation.IsSuccess()) return foundation::Result<Point>::Failure(transformValidation.Failure());
    return foundation::Result<Point>::Success({imagePoint.x * transform.zoom + transform.pan.x, imagePoint.y * transform.zoom + transform.pan.y});
}

foundation::Result<Point> ClampPointToImage(const Point &imagePoint, const ImageSize &imageSize)
{
    const auto pointValidation = ValidatePoint(imagePoint);
    if (!pointValidation.IsSuccess()) return foundation::Result<Point>::Failure(pointValidation.Failure());
    const auto sizeValidation = ValidateImageSize(imageSize);
    if (!sizeValidation.IsSuccess()) return foundation::Result<Point>::Failure(sizeValidation.Failure());
    return foundation::Result<Point>::Success({std::clamp(imagePoint.x, 0.0, static_cast<double>(imageSize.width - 1)), std::clamp(imagePoint.y, 0.0, static_cast<double>(imageSize.height - 1))});
}

foundation::Result<Rect> ClampRectToImage(const Rect &rect, const ImageSize &imageSize)
{
    const auto rectValidation = ValidateRect(rect);
    if (!rectValidation.IsSuccess()) return foundation::Result<Rect>::Failure(rectValidation.Failure());
    const auto sizeValidation = ValidateImageSize(imageSize);
    if (!sizeValidation.IsSuccess()) return foundation::Result<Rect>::Failure(sizeValidation.Failure());
    const double x1 = std::clamp(rect.x, 0.0, static_cast<double>(imageSize.width));
    const double y1 = std::clamp(rect.y, 0.0, static_cast<double>(imageSize.height));
    const double x2 = std::clamp(rect.x + rect.width, 0.0, static_cast<double>(imageSize.width));
    const double y2 = std::clamp(rect.y + rect.height, 0.0, static_cast<double>(imageSize.height));
    const Rect clamped{x1, y1, x2 - x1, y2 - y1};
    const auto clampedValidation = ValidateRect(clamped);
    if (!clampedValidation.IsSuccess()) return foundation::Result<Rect>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Clamped rectangle does not overlap the image"));
    return foundation::Result<Rect>::Success(clamped);
}
}
