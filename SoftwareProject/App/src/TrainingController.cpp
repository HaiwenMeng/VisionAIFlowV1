#include "visionaiflow/app/TrainingController.h"

#include "visionaiflow/annotation/AnnotationStore.h"
#include "visionaiflow/export/OnnxExporter.h"
#include "visionaiflow/models/yolo11/Yolo11Detector.h"
#include "visionaiflow/project_store/DatasetIndex.h"
#include "visionaiflow/project_store/LabelStore.h"

#ifdef slots
#pragma push_macro("slots")
#undef slots
#define VAF_RESTORE_QT_SLOTS_MACRO
#endif
#include <torch/torch.h>
#ifdef VAF_RESTORE_QT_SLOTS_MACRO
#pragma pop_macro("slots")
#undef VAF_RESTORE_QT_SLOTS_MACRO
#endif

#include <ATen/cuda/CUDAContextLight.h>

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSaveFile>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace visionaiflow::app
{
namespace
{
constexpr int kGridInputSize = 640;
constexpr int kGridStride = 8;

struct GridSample final
{
    torch::Tensor image;
    std::vector<models::yolo11::Yolo11GroundTruthDetection> targets;
};

torch::Tensor LoadLetterboxedImage(const QString &path)
{
    QImage source(path);
    if (source.isNull())
    {
        throw std::runtime_error("Project dataset image could not be loaded: " + path.toStdString());
    }

    const QImage rgb = source.convertToFormat(QImage::Format_RGB888);
    QImage canvas(kGridInputSize, kGridInputSize, QImage::Format_RGB888);
    canvas.fill(QColor(114, 114, 114));
    const double scale = std::min(static_cast<double>(kGridInputSize) / static_cast<double>(rgb.width()),
                                  static_cast<double>(kGridInputSize) / static_cast<double>(rgb.height()));
    const QSize scaledSize(static_cast<int>(std::round(static_cast<double>(rgb.width()) * scale)),
                           static_cast<int>(std::round(static_cast<double>(rgb.height()) * scale)));
    QPainter painter(&canvas);
    painter.drawImage((kGridInputSize - scaledSize.width()) / 2,
                      (kGridInputSize - scaledSize.height()) / 2,
                      rgb.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    painter.end();

    std::vector<float> values(static_cast<size_t>(3 * kGridInputSize * kGridInputSize));
    for (int y = 0; y < kGridInputSize; ++y)
    {
        const auto *row = canvas.constScanLine(y);
        for (int x = 0; x < kGridInputSize; ++x)
        {
            const int inputOffset = x * 3;
            const int pixelOffset = y * kGridInputSize + x;
            values[static_cast<size_t>(pixelOffset)] = static_cast<float>(row[inputOffset]) / 255.0F;
            values[static_cast<size_t>(kGridInputSize * kGridInputSize + pixelOffset)] =
                static_cast<float>(row[inputOffset + 1]) / 255.0F;
            values[static_cast<size_t>(2 * kGridInputSize * kGridInputSize + pixelOffset)] =
                static_cast<float>(row[inputOffset + 2]) / 255.0F;
        }
    }

    return torch::from_blob(values.data(),
                            {3, kGridInputSize, kGridInputSize},
                            torch::TensorOptions().dtype(torch::kFloat32))
        .clone();
}

std::vector<models::common::DetectionBox> CreateGridCandidateBoxes()
{
    std::vector<models::common::DetectionBox> boxes;
    boxes.reserve(static_cast<size_t>((kGridInputSize / kGridStride) * (kGridInputSize / kGridStride)));
    for (int y = 0; y < kGridInputSize; y += kGridStride)
    {
        for (int x = 0; x < kGridInputSize; x += kGridStride)
        {
            boxes.push_back({static_cast<float>(x),
                             static_cast<float>(y),
                             static_cast<float>(x + kGridStride),
                             static_cast<float>(y + kGridStride)});
        }
    }
    return boxes;
}

std::vector<GridSample> LoadGridSamples(const QString &projectRoot, int classCount)
{
    const auto labels = project_store::LabelStore().Load(projectRoot);
    if (!labels.IsSuccess())
    {
        throw std::runtime_error(labels.Failure().message);
    }
    if (labels.Value().empty() || static_cast<int>(labels.Value().size()) != classCount)
    {
        throw std::runtime_error("Detection training requires at least one current project label");
    }

    QHash<QString, int> labelIndexes;
    for (int index = 0; index < static_cast<int>(labels.Value().size()); ++index)
    {
        labelIndexes.insert(labels.Value().at(static_cast<size_t>(index)).labelId, index);
    }

    const auto images = project_store::DatasetIndex().Load(projectRoot);
    if (!images.IsSuccess())
    {
        throw std::runtime_error(images.Failure().message);
    }

    annotation::AnnotationStore annotationStore;
    std::vector<GridSample> samples;
    for (const auto &image : images.Value())
    {
        const auto annotations = annotationStore.Load(projectRoot, image.imageId);
        if (!annotations.IsSuccess())
        {
            throw std::runtime_error(annotations.Failure().message);
        }

        std::vector<models::yolo11::Yolo11GroundTruthDetection> targets;
        const float scale = std::min(static_cast<float>(kGridInputSize) / static_cast<float>(image.size.width()),
                                     static_cast<float>(kGridInputSize) / static_cast<float>(image.size.height()));
        const float padX =
            static_cast<float>(kGridInputSize - static_cast<int>(std::round(image.size.width() * scale))) / 2.0F;
        const float padY =
            static_cast<float>(kGridInputSize - static_cast<int>(std::round(image.size.height() * scale))) / 2.0F;
        for (const auto &annotation : annotations.Value())
        {
            if (annotation.kind != annotation::AnnotationKind::BoundingBox)
            {
                continue;
            }

            const auto label = labelIndexes.constFind(annotation.labelId);
            if (label == labelIndexes.cend())
            {
                throw std::runtime_error("Bounding box references an unknown project label");
            }

            const auto &box = annotation.boundingBox;
            targets.push_back({{static_cast<float>(box.x * scale + padX),
                                static_cast<float>(box.y * scale + padY),
                                static_cast<float>((box.x + box.width) * scale + padX),
                                static_cast<float>((box.y + box.height) * scale + padY)},
                               label.value()});
        }
        if (targets.empty())
        {
            continue;
        }

        samples.push_back({LoadLetterboxedImage(QDir(projectRoot).filePath(image.relativePath)), std::move(targets)});
    }

    if (samples.empty())
    {
        throw std::runtime_error("Detection training requires at least one image with a valid bounding box annotation");
    }
    return samples;
}

void RunTraining(TrainingController *controller,
                 const YoloTrainingRequest &request,
                 const std::shared_ptr<std::atomic_bool> &cancel)
{
    if (request.projectRoot.isEmpty() || request.epochs <= 0 || request.batchSize <= 0 || request.learningRate <= 0.0)
    {
        throw std::runtime_error("Training requires projectRoot, positive epochs, batchSize and learningRate");
    }
    if (!torch::cuda::is_available() || at::cuda::getCurrentDeviceProperties() == nullptr)
    {
        throw std::runtime_error("CUDA is unavailable for yolodet training");
    }

    const auto labels = project_store::LabelStore().Load(request.projectRoot);
    if (!labels.IsSuccess())
    {
        throw std::runtime_error(labels.Failure().message);
    }
    if (labels.Value().empty())
    {
        throw std::runtime_error("Detection training requires at least one label");
    }

    const QString runId = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString runRoot = QDir(request.projectRoot).filePath(QStringLiteral("runs/") + runId);
    const QString modelPath =
        QDir(request.projectRoot).filePath(QStringLiteral("models/") + runId + QStringLiteral(".onnx"));
    if (!QDir().mkpath(runRoot) || !QDir().mkpath(QFileInfo(modelPath).dir().absolutePath()))
    {
        throw std::runtime_error("Unable to create detection run output directory");
    }

    QJsonObject configuration;
    configuration.insert(QStringLiteral("modelId"), QStringLiteral("detection.yolo11.grid.v1"));
    configuration.insert(QStringLiteral("inputWidth"), kGridInputSize);
    configuration.insert(QStringLiteral("inputHeight"), kGridInputSize);
    configuration.insert(QStringLiteral("epochs"), request.epochs);
    configuration.insert(QStringLiteral("batchSize"), request.batchSize);
    configuration.insert(QStringLiteral("learningRate"), request.learningRate);
    configuration.insert(QStringLiteral("horizontalFlip"), request.horizontalFlip);
    configuration.insert(QStringLiteral("resumeCheckpointPath"), request.resumeCheckpointPath);
    QSaveFile configurationFile(QDir(runRoot).filePath(QStringLiteral("config.json")));
    if (!configurationFile.open(QIODevice::WriteOnly) ||
        configurationFile.write(QJsonDocument(configuration).toJson(QJsonDocument::Indented)) < 0 ||
        !configurationFile.commit())
    {
        throw std::runtime_error("Unable to write detection run config.json");
    }

    const int classCount = static_cast<int>(labels.Value().size());
    const std::vector<GridSample> samples = LoadGridSamples(request.projectRoot, classCount);
    const auto created = models::yolo11::CreateYolo11GridDetector(3, classCount);
    if (!created.IsSuccess())
    {
        throw std::runtime_error(created.Failure().message);
    }

    auto model = created.Value();
    const torch::Device cuda(torch::kCUDA, 0);
    model->to(cuda);
    torch::optim::AdamW optimizer(model->parameters(), torch::optim::AdamWOptions(request.learningRate));
    if (!request.resumeCheckpointPath.isEmpty())
    {
        training::TrainingCheckpointState state;
        const auto resumed = models::yolo11::LoadYolo11GridDetectorCheckpoint(request.resumeCheckpointPath,
                                                                              model,
                                                                              optimizer,
                                                                              cuda,
                                                                              state);
        if (!resumed.IsSuccess())
        {
            throw std::runtime_error(resumed.Failure().message);
        }
    }

    const auto candidates = CreateGridCandidateBoxes();
    double bestLoss = std::numeric_limits<double>::infinity();
    int step = 0;
    QJsonArray metrics;
    for (int epoch = 0; epoch < request.epochs; ++epoch)
    {
        for (int first = 0; first < static_cast<int>(samples.size()); first += request.batchSize)
        {
            if (cancel->load())
            {
                emit controller->Cancelled();
                return;
            }

            const int last = std::min(first + request.batchSize, static_cast<int>(samples.size()));
            std::vector<torch::Tensor> images;
            std::vector<std::vector<models::yolo11::Yolo11AssignedTarget>> assignments;
            for (int index = first; index < last; ++index)
            {
                auto targets = samples.at(static_cast<size_t>(index)).targets;
                torch::Tensor image = samples.at(static_cast<size_t>(index)).image;
                if (request.horizontalFlip && ((epoch + index) % 2 == 1))
                {
                    const auto flipped =
                        models::yolo11::FlipYolo11DetectionSampleHorizontally(image,
                                                                              targets,
                                                                              classCount,
                                                                              static_cast<float>(kGridInputSize));
                    if (!flipped.IsSuccess())
                    {
                        throw std::runtime_error(flipped.Failure().message);
                    }
                    image = flipped.Value().image;
                    targets = flipped.Value().targets;
                }

                const auto assigned = models::yolo11::AssignYolo11DetectionTargets(candidates, targets, classCount, {});
                if (!assigned.IsSuccess())
                {
                    throw std::runtime_error(assigned.Failure().message);
                }
                images.push_back(image);
                assignments.push_back(assigned.Value());
            }

            models::yolo11::Yolo11DetectionBatch batch{torch::stack(images).to(cuda), std::move(assignments)};
            const auto trained = models::yolo11::TrainYolo11GridDetectionStep(model, optimizer, batch, classCount, {});
            if (!trained.IsSuccess())
            {
                throw std::runtime_error(trained.Failure().message);
            }

            ++step;
            const double loss = trained.Value().totalLoss.detach().to(torch::kCPU).item<double>();
            metrics.append(QJsonObject{{QStringLiteral("epoch"), epoch + 1},
                                       {QStringLiteral("step"), step},
                                       {QStringLiteral("loss"), loss},
                                       {QStringLiteral("boxLoss"), trained.Value().boxLoss},
                                       {QStringLiteral("classLoss"), trained.Value().classLoss}});
            emit controller->Progress(epoch + 1,
                                      step,
                                      loss,
                                      trained.Value().boxLoss,
                                      trained.Value().classLoss,
                                      trained.Value().positiveRows,
                                      trained.Value().meanPositiveIou);

            const auto lastSaved =
                models::yolo11::SaveYolo11GridDetectorCheckpoint(QDir(runRoot).filePath(QStringLiteral("last.pt")),
                                                                 model,
                                                                 optimizer);
            if (!lastSaved.IsSuccess())
            {
                throw std::runtime_error(lastSaved.Failure().message);
            }
            if (loss < bestLoss)
            {
                bestLoss = loss;
                const auto bestSaved =
                    models::yolo11::SaveYolo11GridDetectorCheckpoint(QDir(runRoot).filePath(QStringLiteral("best.pt")),
                                                                     model,
                                                                     optimizer);
                if (!bestSaved.IsSuccess())
                {
                    throw std::runtime_error(bestSaved.Failure().message);
                }
            }
        }
    }

    QSaveFile metricsFile(QDir(runRoot).filePath(QStringLiteral("metrics.json")));
    if (!metricsFile.open(QIODevice::WriteOnly) ||
        metricsFile.write(QJsonDocument(metrics).toJson(QJsonDocument::Indented)) < 0 || !metricsFile.commit())
    {
        throw std::runtime_error("Unable to write training metrics.json");
    }

    const auto exported = exporter::ExportYolo11GridDetectorOnnx(modelPath, model, classCount);
    if (!exported.IsSuccess())
    {
        throw std::runtime_error(exported.Failure().message);
    }

    emit controller->Completed(modelPath, QDir(runRoot).filePath(QStringLiteral("best.pt")));
}
} // namespace

TrainingController::TrainingController(QObject *parent) : QObject(parent)
{
}

TrainingController::~TrainingController()
{
    Cancel();
    if (m_thread != nullptr)
    {
        m_thread->wait();
    }
}

void TrainingController::Start(const YoloTrainingRequest &request)
{
    if (IsRunning())
    {
        emit Failed(QString(u8"训练任务正在运行"));
        return;
    }

    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto cancel = m_cancel;
    auto *thread = QThread::create(
        [this, request, cancel]()
        {
            try
            {
                RunTraining(this, request, cancel);
            }
            catch (const std::exception &error)
            {
                const QString message = QString::fromLocal8Bit(error.what());
                qCritical().noquote() << QString(u8"yolodet训练失败:") << message;
                emit Failed(message);
            }
            catch (...)
            {
                const QString message = QString(u8"yolodet训练发生未知异常");
                qCritical().noquote() << message;
                emit Failed(message);
            }
        });
    m_thread = thread;
    connect(thread,
            &QThread::finished,
            this,
            [this, thread]()
            {
                thread->deleteLater();
                m_thread = nullptr;
                m_cancel.reset();
                emit StateChanged(false);
            });
    emit StateChanged(true);
    thread->start();
}

void TrainingController::Cancel()
{
    if (m_cancel)
    {
        m_cancel->store(true);
    }
}

bool TrainingController::IsRunning() const noexcept
{
    return m_thread != nullptr && m_thread->isRunning();
}
} // namespace visionaiflow::app
