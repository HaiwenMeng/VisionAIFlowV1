#include "visionaiflow/annotation/Geometry.h"

#include <QtTest>

class GeometryTest final : public QObject
{
    Q_OBJECT

private slots:
    void CanonicalizesUndirectedLine();
    void RejectsInvalidPolygon();
    void AcceptsSimplePolygon();
    void MapsViewportCoordinatesWithoutRounding();
    void ValidatesAndClampsImageRectangles();
    void RejectsDegenerateAndOutOfBoundsLines();
};

void GeometryTest::CanonicalizesUndirectedLine()
{
    const visionaiflow::annotation::LineSegment reversed{{10.0, 5.0}, {2.0, 1.0}};
    const auto canonical = visionaiflow::annotation::CanonicalizeLine(reversed);
    QVERIFY(canonical.IsSuccess());
    QCOMPARE(canonical.Value().first.x, 2.0);
    QCOMPARE(canonical.Value().second.x, 10.0);
    QVERIFY(visionaiflow::annotation::IsSameUndirectedLine(reversed, {{2.0, 1.0}, {10.0, 5.0}}, 0.0));
}

void GeometryTest::RejectsInvalidPolygon()
{
    const std::vector<visionaiflow::annotation::Point> selfIntersecting{{0.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}, {2.0, 0.0}};
    const auto validation = visionaiflow::annotation::ValidatePolygon(selfIntersecting);
    QVERIFY(!validation.IsSuccess());
    QVERIFY(!validation.Failure().message.empty());
}

void GeometryTest::AcceptsSimplePolygon()
{
    const std::vector<visionaiflow::annotation::Point> square{{0.0, 0.0}, {3.0, 0.0}, {3.0, 2.0}, {0.0, 2.0}};
    const auto validation = visionaiflow::annotation::ValidatePolygon(square);
    QVERIFY(validation.IsSuccess());
    QCOMPARE(visionaiflow::annotation::SignedPolygonArea(square), 6.0);
}

void GeometryTest::MapsViewportCoordinatesWithoutRounding()
{
    const visionaiflow::annotation::ViewportTransform transform{2.5, {10.0, -4.0}};
    const auto imagePoint = visionaiflow::annotation::ViewportToImage({35.0, 21.0}, transform);
    QVERIFY(imagePoint.IsSuccess());
    QCOMPARE(imagePoint.Value().x, 10.0);
    QCOMPARE(imagePoint.Value().y, 10.0);
    const auto viewportPoint = visionaiflow::annotation::ImageToViewport(imagePoint.Value(), transform);
    QVERIFY(viewportPoint.IsSuccess());
    QCOMPARE(viewportPoint.Value().x, 35.0);
    QCOMPARE(viewportPoint.Value().y, 21.0);
    const auto clamped = visionaiflow::annotation::ClampPointToImage({200.0, -5.0}, {32, 16});
    QVERIFY(clamped.IsSuccess());
    QCOMPARE(clamped.Value().x, 31.0);
    QCOMPARE(clamped.Value().y, 0.0);
}

void GeometryTest::ValidatesAndClampsImageRectangles()
{
    QVERIFY(visionaiflow::annotation::ValidateRectInsideImage({1.0, 2.0, 10.0, 5.0}, {32, 16}).IsSuccess());
    const auto outside = visionaiflow::annotation::ValidateRectInsideImage({30.0, 2.0, 10.0, 5.0}, {32, 16});
    QVERIFY(!outside.IsSuccess());
    QVERIFY(!outside.Failure().message.empty());
    const auto clamped = visionaiflow::annotation::ClampRectToImage({-2.0, 3.0, 8.0, 20.0}, {10, 12});
    QVERIFY(clamped.IsSuccess());
    QCOMPARE(clamped.Value().x, 0.0);
    QCOMPARE(clamped.Value().y, 3.0);
    QCOMPARE(clamped.Value().width, 6.0);
    QCOMPARE(clamped.Value().height, 9.0);
    const auto disjoint = visionaiflow::annotation::ClampRectToImage({12.0, 0.0, 3.0, 3.0}, {10, 10});
    QVERIFY(!disjoint.IsSuccess());
    QVERIFY(!disjoint.Failure().message.empty());
}

void GeometryTest::RejectsDegenerateAndOutOfBoundsLines()
{
    const auto zeroLength = visionaiflow::annotation::CanonicalizeLine({{4.0, 4.0}, {4.0, 4.0}});
    QVERIFY(!zeroLength.IsSuccess());
    QVERIFY(!zeroLength.Failure().message.empty());
    const auto tiny = visionaiflow::annotation::CanonicalizeLine({{0.0, 0.0}, {1.0e-9, 0.0}});
    QVERIFY(!tiny.IsSuccess());
    QVERIFY(!tiny.Failure().message.empty());
    QVERIFY(visionaiflow::annotation::ValidateLineInsideImage({{0.0, 0.0}, {31.0, 15.0}}, {32, 16}).IsSuccess());
    const auto outside = visionaiflow::annotation::ValidateLineInsideImage({{0.0, 0.0}, {32.0, 15.0}}, {32, 16});
    QVERIFY(!outside.IsSuccess());
    QVERIFY(!outside.Failure().message.empty());
}

QTEST_APPLESS_MAIN(GeometryTest)

#include "tst_Geometry.moc"
