#include "visionaiflow/model_graph/ModelGraph.h"

#include <QtTest>

class ModelGraphTest final : public QObject
{
    Q_OBJECT

private slots:
    void InfersConvClassifierShape();
    void InfersCommonOperatorShapes();
    void RejectsInvalidConvShape();
    void RejectsInvalidOperatorParameters();
};

void ModelGraphTest::InfersConvClassifierShape()
{
    visionaiflow::model_graph::ModelGraph graph;
    QVERIFY(graph.AddNode({visionaiflow::model_graph::NodeKind::Conv2d, 8, 3, 2, 1}).IsSuccess());
    QVERIFY(graph.AddNode({visionaiflow::model_graph::NodeKind::Relu}).IsSuccess());
    QVERIFY(graph.AddNode({visionaiflow::model_graph::NodeKind::Flatten, 0, 0, 1, 0, 0, 1}).IsSuccess());
    QVERIFY(graph.AddNode({visionaiflow::model_graph::NodeKind::Linear, 0, 0, 1, 0, 5}).IsSuccess());
    const auto output = graph.InferOutputShape({{2, 3, 32, 32}});
    QVERIFY(output.IsSuccess());
    QCOMPARE(output.Value().dimensions, std::vector<int64_t>({2, 5}));
}

void ModelGraphTest::InfersCommonOperatorShapes()
{
    using namespace visionaiflow::model_graph;

    GraphNode maxPool{NodeKind::MaxPool};
    maxPool.kernelSize = 2;
    maxPool.stride = 2;
    auto output = InferNodeShape({{2, 8, 32, 32}}, maxPool);
    QVERIFY2(output.IsSuccess(), output.IsSuccess() ? "" : output.Failure().message.c_str());
    QCOMPARE(output.Value().dimensions, std::vector<int64_t>({2, 8, 16, 16}));

    GraphNode averagePool{NodeKind::AveragePool};
    averagePool.kernelSize = 3;
    averagePool.stride = 1;
    averagePool.padding = 1;
    output = InferNodeShape({{2, 8, 16, 16}}, averagePool);
    QVERIFY(output.IsSuccess());
    QCOMPARE(output.Value().dimensions, std::vector<int64_t>({2, 8, 16, 16}));

    GraphNode adaptivePool{NodeKind::AdaptiveAveragePool};
    adaptivePool.outputShape = {1, 1};
    output = InferNodeShape({{2, 8, 16, 16}}, adaptivePool);
    QVERIFY(output.IsSuccess());
    QCOMPARE(output.Value().dimensions, std::vector<int64_t>({2, 8, 1, 1}));

    GraphNode reshape{NodeKind::Reshape};
    reshape.outputShape = {2, -1};
    output = InferNodeShape({{2, 8, 1, 1}}, reshape);
    QVERIFY(output.IsSuccess());
    QCOMPARE(output.Value().dimensions, std::vector<int64_t>({2, 8}));

    GraphNode transpose{NodeKind::Transpose};
    transpose.permutation = {0, 2, 3, 1};
    output = InferNodeShape({{2, 8, 4, 5}}, transpose);
    QVERIFY(output.IsSuccess());
    QCOMPARE(output.Value().dimensions, std::vector<int64_t>({2, 4, 5, 8}));

    GraphNode squeeze{NodeKind::Squeeze};
    squeeze.axes = {2, 3};
    output = InferNodeShape({{2, 8, 1, 1}}, squeeze);
    QVERIFY(output.IsSuccess());
    QCOMPARE(output.Value().dimensions, std::vector<int64_t>({2, 8}));

    GraphNode unsqueeze{NodeKind::Unsqueeze};
    unsqueeze.axes = {1, 3};
    output = InferNodeShape({{2, 8}}, unsqueeze);
    QVERIFY(output.IsSuccess());
    QCOMPARE(output.Value().dimensions, std::vector<int64_t>({2, 1, 8, 1}));

    GraphNode reduce{NodeKind::ReduceMean};
    reduce.axes = {2, 3};
    reduce.keepDimensions = false;
    output = InferNodeShape({{2, 8, 4, 5}}, reduce);
    QVERIFY(output.IsSuccess());
    QCOMPARE(output.Value().dimensions, std::vector<int64_t>({2, 8}));

    output = InferNodeShape({{2, 8}}, {NodeKind::BatchNorm});
    QVERIFY(output.IsSuccess());
    QCOMPARE(output.Value().dimensions, std::vector<int64_t>({2, 8}));

    GraphNode softmax{NodeKind::Softmax};
    softmax.axes = {1};
    output = InferNodeShape({{2, 8}}, softmax);
    QVERIFY(output.IsSuccess());
    QCOMPARE(output.Value().dimensions, std::vector<int64_t>({2, 8}));
}

void ModelGraphTest::RejectsInvalidConvShape()
{
    const auto result = visionaiflow::model_graph::InferNodeShape({{1, 3, 2, 2}}, {visionaiflow::model_graph::NodeKind::Conv2d, 4, 5, 1, 0});
    QVERIFY(!result.IsSuccess());
    QVERIFY(!result.Failure().message.empty());
}

void ModelGraphTest::RejectsInvalidOperatorParameters()
{
    using namespace visionaiflow::model_graph;

    GraphNode reshape{NodeKind::Reshape};
    reshape.outputShape = {5, 5};
    auto result = InferNodeShape({{2, 3, 4}}, reshape);
    QVERIFY(!result.IsSuccess());
    QVERIFY(!result.Failure().message.empty());

    GraphNode transpose{NodeKind::Transpose};
    transpose.permutation = {0, 0};
    result = InferNodeShape({{2, 3}}, transpose);
    QVERIFY(!result.IsSuccess());
    QVERIFY(!result.Failure().message.empty());

    GraphNode squeeze{NodeKind::Squeeze};
    squeeze.axes = {1};
    result = InferNodeShape({{2, 3}}, squeeze);
    QVERIFY(!result.IsSuccess());
    QVERIFY(!result.Failure().message.empty());

    GraphNode reduce{NodeKind::ReduceMean};
    reduce.keepDimensions = false;
    result = InferNodeShape({{2, 3}}, reduce);
    QVERIFY(!result.IsSuccess());
    QVERIFY(!result.Failure().message.empty());
}

QTEST_APPLESS_MAIN(ModelGraphTest)

#include "tst_ModelGraph.moc"
