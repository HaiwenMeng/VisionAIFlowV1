#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace visionaiflow::yolov11
{

constexpr int Yolo11MetricIouCount = 10;

struct Yolo11Box
{
    float x1{0.0f};
    float y1{0.0f};
    float x2{0.0f};
    float y2{0.0f};
};

struct Yolo11Detection
{
    Yolo11Box box;
    float confidence{0.0f};
    int classId{-1};
};

struct Yolo11GroundTruth
{
    Yolo11Box box;
    int classId{-1};
};

struct Yolo11ValidationMetrics
{
    // Ultralytics-style AP metrics.
    double map50{0.0};
    double map5095{0.0};

    // Size == classCount. Classes absent from validation GT remain 0.
    std::vector<double> ap50PerClass;
    std::vector<int64_t> groundTruthPerClass;

    int64_t imageCount{0};
    int64_t predictionCount{0};
    int64_t groundTruthCount{0};
};

class Yolo11Metrics
{
public:
    Yolo11Metrics();

    void clear();

    // Add one image after decode + class-aware NMS.
    // Matching follows Ultralytics DetectionValidator semantics:
    // same class, IoU thresholds 0.50:0.05:0.95, one-to-one matching.
    void addImage(const std::vector<Yolo11Detection> &predictions,
                  const std::vector<Yolo11GroundTruth> &targets);

    Yolo11ValidationMetrics compute(int classCount) const;

private:
    struct PredictionStat
    {
        std::array<uint8_t, Yolo11MetricIouCount> correct{};
        float confidence{0.0f};
        int classId{-1};
    };

    static float boxIou(const Yolo11Box &a, const Yolo11Box &b);
    static double computeAp(const std::vector<double> &recall,
                            const std::vector<double> &precision);
    static double interpolate(const std::vector<double> &x,
                              const std::vector<double> &y,
                              double value);

private:
    std::vector<PredictionStat> m_predictions;
    std::vector<int> m_targetClasses;
    int64_t m_imageCount{0};
};

} // namespace visionaiflow::yolov11
