#include "yolov11metrics.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_set>

namespace visionaiflow::yolov11
{
namespace
{
constexpr double kEpsilon = 1e-16;
constexpr std::array<float, Yolo11MetricIouCount> kIouThresholds{
    0.50f, 0.55f, 0.60f, 0.65f, 0.70f,
    0.75f, 0.80f, 0.85f, 0.90f, 0.95f};

struct Match
{
    int groundTruthIndex{-1};
    int predictionIndex{-1};
    float iou{0.0f};
};
}

Yolo11Metrics::Yolo11Metrics() = default;

void Yolo11Metrics::clear()
{
    m_predictions.clear();
    m_targetClasses.clear();
    m_imageCount = 0;
}

float Yolo11Metrics::boxIou(const Yolo11Box &a, const Yolo11Box &b)
{
    const float interLeft = std::max(a.x1, b.x1);
    const float interTop = std::max(a.y1, b.y1);
    const float interRight = std::min(a.x2, b.x2);
    const float interBottom = std::min(a.y2, b.y2);

    const float interWidth = std::max(0.0f, interRight - interLeft);
    const float interHeight = std::max(0.0f, interBottom - interTop);
    const float intersection = interWidth * interHeight;

    const float areaA = std::max(0.0f, a.x2 - a.x1) *
                        std::max(0.0f, a.y2 - a.y1);
    const float areaB = std::max(0.0f, b.x2 - b.x1) *
                        std::max(0.0f, b.y2 - b.y1);
    const float unionArea = areaA + areaB - intersection;

    return unionArea > 0.0f ? intersection / unionArea : 0.0f;
}

void Yolo11Metrics::addImage(const std::vector<Yolo11Detection> &predictions,
                             const std::vector<Yolo11GroundTruth> &targets)
{
    ++m_imageCount;

    for (const Yolo11GroundTruth &target : targets)
    {
        m_targetClasses.push_back(target.classId);
    }

    std::vector<PredictionStat> imageStats(predictions.size());
    for (size_t predictionIndex = 0; predictionIndex < predictions.size(); ++predictionIndex)
    {
        imageStats[predictionIndex].confidence = predictions[predictionIndex].confidence;
        imageStats[predictionIndex].classId = predictions[predictionIndex].classId;
    }

    if (!predictions.empty() && !targets.empty())
    {
        // Match independently for each IoU threshold, exactly as required for
        // AP50 and AP50:95. Sorting is by IoU, not confidence; predictions
        // have already passed NMS before reaching this function.
        for (int iouIndex = 0; iouIndex < Yolo11MetricIouCount; ++iouIndex)
        {
            const float threshold = kIouThresholds[static_cast<size_t>(iouIndex)];
            std::vector<Match> matches;

            for (int targetIndex = 0; targetIndex < static_cast<int>(targets.size()); ++targetIndex)
            {
                for (int predictionIndex = 0;
                     predictionIndex < static_cast<int>(predictions.size());
                     ++predictionIndex)
                {
                    if (targets[static_cast<size_t>(targetIndex)].classId !=
                        predictions[static_cast<size_t>(predictionIndex)].classId)
                    {
                        continue;
                    }

                    const float iou = boxIou(
                        targets[static_cast<size_t>(targetIndex)].box,
                        predictions[static_cast<size_t>(predictionIndex)].box);

                    if (iou >= threshold)
                    {
                        matches.push_back({targetIndex, predictionIndex, iou});
                    }
                }
            }

            if (matches.empty())
            {
                continue;
            }

            // Ultralytics 8.3.x match_predictions():
            // 1) sort IoU descending
            // 2) unique prediction
            // 3) sort IoU descending again
            // 4) unique GT
            std::sort(matches.begin(), matches.end(),
                      [](const Match &a, const Match &b) { return a.iou > b.iou; });

            std::unordered_set<int> usedPredictions;
            std::vector<Match> predictionUnique;
            predictionUnique.reserve(matches.size());
            for (const Match &match : matches)
            {
                if (usedPredictions.insert(match.predictionIndex).second)
                {
                    predictionUnique.push_back(match);
                }
            }

            std::sort(predictionUnique.begin(), predictionUnique.end(),
                      [](const Match &a, const Match &b) { return a.iou > b.iou; });

            std::unordered_set<int> usedTargets;
            for (const Match &match : predictionUnique)
            {
                if (usedTargets.insert(match.groundTruthIndex).second)
                {
                    imageStats[static_cast<size_t>(match.predictionIndex)]
                        .correct[static_cast<size_t>(iouIndex)] = 1;
                }
            }
        }
    }

    m_predictions.insert(m_predictions.end(), imageStats.begin(), imageStats.end());
}

double Yolo11Metrics::interpolate(const std::vector<double> &x,
                                  const std::vector<double> &y,
                                  const double value)
{
    if (x.empty() || y.empty() || x.size() != y.size())
    {
        return 0.0;
    }

    if (value <= x.front())
    {
        return y.front();
    }
    if (value >= x.back())
    {
        return y.back();
    }

    // upper_bound intentionally chooses the last value when x contains
    // duplicate recall coordinates, which is appropriate for the precision
    // envelope used by Ultralytics AP interpolation.
    const auto upper = std::upper_bound(x.begin(), x.end(), value);
    const size_t right = static_cast<size_t>(std::distance(x.begin(), upper));
    const size_t left = right - 1;

    const double x0 = x[left];
    const double x1 = x[right];
    const double y0 = y[left];
    const double y1 = y[right];

    if (std::abs(x1 - x0) < kEpsilon)
    {
        return y1;
    }

    const double t = (value - x0) / (x1 - x0);
    return y0 + t * (y1 - y0);
}

double Yolo11Metrics::computeAp(const std::vector<double> &recall,
                                const std::vector<double> &precision)
{
    if (recall.empty() || precision.empty() || recall.size() != precision.size())
    {
        return 0.0;
    }

    // Ultralytics 8.3.4-style sentinels.
    std::vector<double> mrec;
    std::vector<double> mpre;
    mrec.reserve(recall.size() + 2);
    mpre.reserve(precision.size() + 2);

    mrec.push_back(0.0);
    mrec.insert(mrec.end(), recall.begin(), recall.end());
    mrec.push_back(1.0);

    mpre.push_back(1.0);
    mpre.insert(mpre.end(), precision.begin(), precision.end());
    mpre.push_back(0.0);

    // Precision envelope: reverse cumulative maximum.
    for (int index = static_cast<int>(mpre.size()) - 2; index >= 0; --index)
    {
        mpre[static_cast<size_t>(index)] =
            std::max(mpre[static_cast<size_t>(index)],
                     mpre[static_cast<size_t>(index + 1)]);
    }

    // Ultralytics 'interp' method: interpolate at 101 recall points and
    // integrate using the trapezoidal rule.
    double ap = 0.0;
    double previousX = 0.0;
    double previousY = interpolate(mrec, mpre, 0.0);

    for (int index = 1; index <= 100; ++index)
    {
        const double currentX = static_cast<double>(index) / 100.0;
        const double currentY = interpolate(mrec, mpre, currentX);
        ap += (previousY + currentY) * (currentX - previousX) * 0.5;
        previousX = currentX;
        previousY = currentY;
    }

    return ap;
}

Yolo11ValidationMetrics Yolo11Metrics::compute(const int classCount) const
{
    Yolo11ValidationMetrics result;
    result.imageCount = m_imageCount;
    result.predictionCount = static_cast<int64_t>(m_predictions.size());
    result.groundTruthCount = static_cast<int64_t>(m_targetClasses.size());

    if (classCount <= 0)
    {
        return result;
    }

    result.ap50PerClass.assign(static_cast<size_t>(classCount), 0.0);
    result.groundTruthPerClass.assign(static_cast<size_t>(classCount), 0);

    for (const int classId : m_targetClasses)
    {
        if (classId >= 0 && classId < classCount)
        {
            ++result.groundTruthPerClass[static_cast<size_t>(classId)];
        }
    }

    // Global confidence sort, then each class keeps that order.
    std::vector<size_t> predictionOrder(m_predictions.size());
    std::iota(predictionOrder.begin(), predictionOrder.end(), static_cast<size_t>(0));
    std::sort(predictionOrder.begin(), predictionOrder.end(),
              [this](const size_t a, const size_t b)
              {
                  return m_predictions[a].confidence > m_predictions[b].confidence;
              });

    double map50Sum = 0.0;
    double map5095Sum = 0.0;
    int classesWithGroundTruth = 0;

    for (int classId = 0; classId < classCount; ++classId)
    {
        const int64_t targetCount =
            result.groundTruthPerClass[static_cast<size_t>(classId)];
        if (targetCount <= 0)
        {
            continue;
        }

        ++classesWithGroundTruth;

        std::vector<size_t> classPredictions;
        for (const size_t predictionIndex : predictionOrder)
        {
            if (m_predictions[predictionIndex].classId == classId)
            {
                classPredictions.push_back(predictionIndex);
            }
        }

        std::array<double, Yolo11MetricIouCount> classAp{};

        for (int iouIndex = 0; iouIndex < Yolo11MetricIouCount; ++iouIndex)
        {
            if (classPredictions.empty())
            {
                classAp[static_cast<size_t>(iouIndex)] = 0.0;
                continue;
            }

            std::vector<double> recall;
            std::vector<double> precision;
            recall.reserve(classPredictions.size());
            precision.reserve(classPredictions.size());

            double truePositiveCumulative = 0.0;
            double falsePositiveCumulative = 0.0;

            for (const size_t predictionIndex : classPredictions)
            {
                const bool correct =
                    m_predictions[predictionIndex]
                        .correct[static_cast<size_t>(iouIndex)] != 0;

                if (correct)
                {
                    truePositiveCumulative += 1.0;
                }
                else
                {
                    falsePositiveCumulative += 1.0;
                }

                recall.push_back(truePositiveCumulative /
                                 (static_cast<double>(targetCount) + kEpsilon));
                precision.push_back(truePositiveCumulative /
                                    (truePositiveCumulative +
                                     falsePositiveCumulative + kEpsilon));
            }

            classAp[static_cast<size_t>(iouIndex)] = computeAp(recall, precision);
        }

        result.ap50PerClass[static_cast<size_t>(classId)] = classAp[0];
        map50Sum += classAp[0];

        const double classMap5095 =
            std::accumulate(classAp.begin(), classAp.end(), 0.0) /
            static_cast<double>(Yolo11MetricIouCount);
        map5095Sum += classMap5095;
    }

    if (classesWithGroundTruth > 0)
    {
        result.map50 = map50Sum / static_cast<double>(classesWithGroundTruth);
        result.map5095 = map5095Sum / static_cast<double>(classesWithGroundTruth);
    }

    return result;
}

} // namespace visionaiflow::yolov11
