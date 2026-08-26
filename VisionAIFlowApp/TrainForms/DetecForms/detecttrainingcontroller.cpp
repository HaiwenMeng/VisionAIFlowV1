#include "detecttrainingcontroller.h"

#include "taskrepository.h"
#include "ytyolodefine.h"
#include "visionaiflow/export/OnnxExporter.h"
#include "visionaiflow/models/yolo11/Yolo11Detector.h"

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

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSaveFile>
#include <QTextStream>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace
{
constexpr int kInputSize = 640;
constexpr int kOutputStride = 8;

struct GridSample
{
    torch::Tensor image;
    std::vector<visionaiflow::models::yolo11::Yolo11GroundTruthDetection> targets;
};

torch::Tensor LoadLetterboxedImage(const QString &path)
{
    QImage source(path);
    if (source.isNull())
    {
        throw std::runtime_error("Unable to load dataset image: " + path.toStdString());
    }

    const QImage rgb = source.convertToFormat(QImage::Format_RGB888);
    QImage canvas(kInputSize, kInputSize, QImage::Format_RGB888);
    canvas.fill(QColor(114, 114, 114));
    const double scale =
        std::min(static_cast<double>(kInputSize) / rgb.width(), static_cast<double>(kInputSize) / rgb.height());
    const QSize scaledSize(static_cast<int>(std::round(rgb.width() * scale)),
                           static_cast<int>(std::round(rgb.height() * scale)));
    QPainter painter(&canvas);
    painter.drawImage((kInputSize - scaledSize.width()) / 2,
                      (kInputSize - scaledSize.height()) / 2,
                      rgb.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    painter.end();

    std::vector<float> values(static_cast<size_t>(3 * kInputSize * kInputSize));
    for (int y = 0; y < kInputSize; ++y)
    {
        const auto *row = canvas.constScanLine(y);
        for (int x = 0; x < kInputSize; ++x)
        {
            const int offset = y * kInputSize + x;
            values.at(static_cast<size_t>(offset)) = static_cast<float>(row[x * 3]) / 255.0F;
            values.at(static_cast<size_t>(kInputSize * kInputSize + offset)) =
                static_cast<float>(row[x * 3 + 1]) / 255.0F;
            values.at(static_cast<size_t>(2 * kInputSize * kInputSize + offset)) =
                static_cast<float>(row[x * 3 + 2]) / 255.0F;
        }
    }
    return torch::from_blob(values.data(), {3, kInputSize, kInputSize}, torch::TensorOptions().dtype(torch::kFloat32))
        .clone();
}

std::vector<visionaiflow::models::common::DetectionBox> CreateCandidateBoxes()
{
    std::vector<visionaiflow::models::common::DetectionBox> boxes;
    boxes.reserve(static_cast<size_t>((kInputSize / kOutputStride) * (kInputSize / kOutputStride)));
    for (int y = 0; y < kInputSize; y += kOutputStride)
    {
        for (int x = 0; x < kInputSize; x += kOutputStride)
        {
            boxes.push_back({static_cast<float>(x),
                             static_cast<float>(y),
                             static_cast<float>(x + kOutputStride),
                             static_cast<float>(y + kOutputStride)});
        }
    }
    return boxes;
}

std::vector<visionaiflow::models::yolo11::Yolo11GroundTruthDetection>
LoadTargets(const QString &labelPath, const QSize &imageSize, int classCount)
{
    QFile file(labelPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        throw std::runtime_error("Unable to read YOLO label file: " + labelPath.toStdString());
    }

    const float scale = std::min(static_cast<float>(kInputSize) / imageSize.width(),
                                 static_cast<float>(kInputSize) / imageSize.height());
    const float padX = static_cast<float>(kInputSize - static_cast<int>(std::round(imageSize.width() * scale))) / 2.0F;
    const float padY = static_cast<float>(kInputSize - static_cast<int>(std::round(imageSize.height() * scale))) / 2.0F;
    std::vector<visionaiflow::models::yolo11::Yolo11GroundTruthDetection> targets;
    QTextStream stream(&file);
    while (!stream.atEnd())
    {
        int classIndex = -1;
        double centerX = 0.0;
        double centerY = 0.0;
        double width = 0.0;
        double height = 0.0;
        stream >> classIndex >> centerX >> centerY >> width >> height;
        if (stream.status() != QTextStream::Ok && !stream.atEnd())
        {
            throw std::runtime_error("Invalid YOLO label content: " + labelPath.toStdString());
        }
        stream.readLine();
        if (classIndex < 0 || classIndex >= classCount || centerX <= 0.0 || centerX >= 1.0 || centerY <= 0.0 ||
            centerY >= 1.0 || width <= 0.0 || width > 1.0 || height <= 0.0 || height > 1.0)
        {
            throw std::runtime_error("YOLO label value is outside the supported range: " + labelPath.toStdString());
        }
        const float sourceCenterX = static_cast<float>(centerX * imageSize.width());
        const float sourceCenterY = static_cast<float>(centerY * imageSize.height());
        const float sourceWidth = static_cast<float>(width * imageSize.width());
        const float sourceHeight = static_cast<float>(height * imageSize.height());
        targets.push_back({{(sourceCenterX - sourceWidth * 0.5F) * scale + padX,
                            (sourceCenterY - sourceHeight * 0.5F) * scale + padY,
                            (sourceCenterX + sourceWidth * 0.5F) * scale + padX,
                            (sourceCenterY + sourceHeight * 0.5F) * scale + padY},
                           classIndex});
    }
    return targets;
}

std::vector<GridSample> LoadSamples(const QString &taskName, int classCount)
{
    const QString csvPath = QDir(YtYoloDefine::toGetDataPath()).filePath(taskName + QStringLiteral("/train.csv"));
    QFile csvFile(csvPath);
    if (!csvFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        throw std::runtime_error("Unable to open generated train.csv: " + csvPath.toStdString());
    }

    std::vector<GridSample> samples;
    QTextStream stream(&csvFile);
    int lineNumber = 0;
    while (!stream.atEnd())
    {
        ++lineNumber;
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty())
        {
            continue;
        }

        const int commaIndex = line.indexOf(QLatin1Char(','));
        if (commaIndex <= 0 || commaIndex >= line.size() - 1)
        {
            throw std::runtime_error("Invalid train.csv row " + std::to_string(lineNumber));
        }

        const QString imagePath = line.left(commaIndex).trimmed();
        const QString labelPath = line.mid(commaIndex + 1).trimmed();
        QImage image(imagePath);
        if (image.isNull())
        {
            throw std::runtime_error("Unable to load training image: " + imagePath.toStdString());
        }
        std::vector<visionaiflow::models::yolo11::Yolo11GroundTruthDetection> targets;
        if (labelPath != QStringLiteral("SIGNAL_BACKGROUND_IMG"))
        {
            targets = LoadTargets(labelPath, image.size(), classCount);
        }
        samples.push_back({LoadLetterboxedImage(imagePath), std::move(targets)});
    }
    if (samples.empty())
    {
        throw std::runtime_error("Detect training requires at least one row in train.csv");
    }
    return samples;
}

bool WriteJson(const QString &path, const QJsonDocument &document)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(document.toJson(QJsonDocument::Indented)) >= 0 &&
           file.commit();
}

void RunTraining(DetectTrainingController *controller,
                 const DetectTrainingRequest &request,
                 const std::shared_ptr<std::atomic_bool> &cancel)
{
    if (request.taskName.isEmpty() || request.epochs <= 0 || request.batchSize <= 0 || request.learningRate <= 0.0)
    {
        throw std::runtime_error("Training requires task name, positive epochs, batch size and learning rate");
    }
    if (!torch::cuda::is_available() || at::cuda::getCurrentDeviceProperties() == nullptr)
    {
        throw std::runtime_error("CUDA is unavailable for Yolo11 grid training");
    }

    TaskDefinition task;
    QString errorMessage;
    if (!TaskRepository::LoadTask(request.taskName, &task, &errorMessage) || task.labels.isEmpty())
    {
        throw std::runtime_error(errorMessage.isEmpty() ? "Detect training requires at least one label"
                                                        : errorMessage.toStdString());
    }
    const int classCount = task.labels.size();
    const std::vector<GridSample> samples = LoadSamples(request.taskName, classCount);
    const QString runDirectory =
        QDir(YtYoloDefine::toGetTrainPath()).filePath(request.taskName + QStringLiteral("/detect/train"));
    const QString weightsDirectory = QDir(runDirectory).filePath(QStringLiteral("weights"));
    if (!QDir().mkpath(weightsDirectory))
    {
        throw std::runtime_error("Unable to create training output directory");
    }

    const QJsonObject configuration{{QStringLiteral("modelId"), QStringLiteral("detection.yolo11.grid.v1")},
                                    {QStringLiteral("inputWidth"), kInputSize},
                                    {QStringLiteral("inputHeight"), kInputSize},
                                    {QStringLiteral("epochs"), request.epochs},
                                    {QStringLiteral("batchSize"), request.batchSize},
                                    {QStringLiteral("learningRate"), request.learningRate},
                                    {QStringLiteral("horizontalFlip"), request.horizontalFlip},
                                    {QStringLiteral("resumeCheckpointPath"), request.resumeCheckpointPath}};
    if (!WriteJson(QDir(runDirectory).filePath(QStringLiteral("config.json")), QJsonDocument(configuration)))
    {
        throw std::runtime_error("Unable to write config.json");
    }

    const auto created = visionaiflow::models::yolo11::CreateYolo11GridDetector(3, classCount);
    if (!created.IsSuccess())
    {
        throw std::runtime_error(created.Failure().message);
    }
    auto model = created.Value();
    const torch::Device device(torch::kCUDA, 0);
    model->to(device);
    torch::optim::AdamW optimizer(model->parameters(), torch::optim::AdamWOptions(request.learningRate));
    if (!request.resumeCheckpointPath.isEmpty())
    {
        visionaiflow::training::TrainingCheckpointState state;
        const auto loaded = visionaiflow::models::yolo11::LoadYolo11GridDetectorCheckpoint(request.resumeCheckpointPath,
                                                                                           model,
                                                                                           optimizer,
                                                                                           device,
                                                                                           state);
        if (!loaded.IsSuccess())
        {
            throw std::runtime_error(loaded.Failure().message);
        }
    }

    const auto candidates = CreateCandidateBoxes();
    QJsonArray metrics;
    double bestLoss = std::numeric_limits<double>::infinity();
    int step = 0;
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
            std::vector<std::vector<visionaiflow::models::yolo11::Yolo11AssignedTarget>> assignments;
            for (int index = first; index < last; ++index)
            {
                torch::Tensor image = samples.at(static_cast<size_t>(index)).image;
                auto targets = samples.at(static_cast<size_t>(index)).targets;
                if (request.horizontalFlip && ((epoch + index) % 2 == 1))
                {
                    const auto flipped = visionaiflow::models::yolo11::FlipYolo11DetectionSampleHorizontally(
                        image,
                        targets,
                        classCount,
                        static_cast<float>(kInputSize));
                    if (!flipped.IsSuccess())
                    {
                        throw std::runtime_error(flipped.Failure().message);
                    }
                    image = flipped.Value().image;
                    targets = flipped.Value().targets;
                }
                const auto assigned =
                    visionaiflow::models::yolo11::AssignYolo11DetectionTargets(candidates, targets, classCount, {});
                if (!assigned.IsSuccess())
                {
                    throw std::runtime_error(assigned.Failure().message);
                }
                images.push_back(image);
                assignments.push_back(assigned.Value());
            }
            visionaiflow::models::yolo11::Yolo11DetectionBatch batch{torch::stack(images).to(device),
                                                                     std::move(assignments)};
            const auto trained =
                visionaiflow::models::yolo11::TrainYolo11GridDetectionStep(model, optimizer, batch, classCount, {});
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

            const auto lastSaved = visionaiflow::models::yolo11::SaveYolo11GridDetectorCheckpoint(
                QDir(weightsDirectory).filePath(QStringLiteral("last.pt")),
                model,
                optimizer);
            if (!lastSaved.IsSuccess())
            {
                throw std::runtime_error(lastSaved.Failure().message);
            }
            if (loss < bestLoss)
            {
                bestLoss = loss;
                const auto bestSaved = visionaiflow::models::yolo11::SaveYolo11GridDetectorCheckpoint(
                    QDir(weightsDirectory).filePath(QStringLiteral("best.pt")),
                    model,
                    optimizer);
                if (!bestSaved.IsSuccess())
                {
                    throw std::runtime_error(bestSaved.Failure().message);
                }
                const auto pretrainedSaved = visionaiflow::models::yolo11::SaveYolo11GridDetectorCheckpoint(
                    QDir(YtYoloDefine::toGetTrainPath())
                        .filePath(request.taskName + QStringLiteral("/YtPretrained.pt")),
                    model,
                    optimizer);
                if (!pretrainedSaved.IsSuccess())
                {
                    throw std::runtime_error(pretrainedSaved.Failure().message);
                }
            }
        }
    }
    if (!WriteJson(QDir(runDirectory).filePath(QStringLiteral("metrics.json")), QJsonDocument(metrics)))
    {
        throw std::runtime_error("Unable to write metrics.json");
    }
    visionaiflow::training::TrainingCheckpointState bestState;
    const auto bestLoaded = visionaiflow::models::yolo11::LoadYolo11GridDetectorCheckpoint(
        QDir(weightsDirectory).filePath(QStringLiteral("best.pt")),
        model,
        optimizer,
        device,
        bestState);
    if (!bestLoaded.IsSuccess())
    {
        throw std::runtime_error(bestLoaded.Failure().message);
    }
    const QString modelPath = QDir(weightsDirectory).filePath(QStringLiteral("best.onnx"));
    const auto exported = visionaiflow::exporter::ExportYolo11GridDetectorOnnx(modelPath, model, classCount);
    if (!exported.IsSuccess())
    {
        throw std::runtime_error(exported.Failure().message);
    }
    emit controller->Completed(runDirectory, modelPath, QDir(weightsDirectory).filePath(QStringLiteral("best.pt")));
}
} // namespace

DetectTrainingController::DetectTrainingController(QObject *parent) : QObject(parent)
{
}

DetectTrainingController::~DetectTrainingController()
{
    Cancel();
    if (m_thread != nullptr)
    {
        m_thread->wait();
    }
}

void DetectTrainingController::Start(const DetectTrainingRequest &request)
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
            catch (const c10::Error &error)
            {
                const QString message = QString(u8"LibTorch 训练失败: %1").arg(QString::fromLocal8Bit(error.what()));
                qCritical().noquote() << message;
                emit Failed(message);
            }
            catch (const std::exception &error)
            {
                const QString message = QString::fromLocal8Bit(error.what());
                qCritical().noquote() << QString(u8"Yolo11 训练失败:") << message;
                emit Failed(message);
            }
            catch (...)
            {
                const QString message = QString(u8"Yolo11 训练发生未知异常");
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

void DetectTrainingController::Cancel()
{
    if (m_cancel)
    {
        m_cancel->store(true);
    }
}

bool DetectTrainingController::IsRunning() const noexcept
{
    return m_thread != nullptr && m_thread->isRunning();
}
