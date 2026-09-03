#include "yolov11plugin.h"

#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>

#include <torch/csrc/jit/frontend/tracer.h>
#include <torch/csrc/jit/serialization/export.h>

#include <chrono>
#include <array>
#include <map>
#include <limits>
#include <string>
#include <unordered_map>

namespace visionaiflow::yolov11
{
namespace
{
constexpr int kInputChannels = 3;
constexpr int kRegMax = 16;

void AppendVarint(QByteArray *data, quint64 value)
{
    while (value >= 0x80U)
    {
        data->append(static_cast<char>((value & 0x7FU) | 0x80U));
        value >>= 7U;
    }
    data->append(static_cast<char>(value));
}

void AppendLengthDelimitedField(QByteArray *data, const quint32 fieldNumber, const QByteArray &value)
{
    AppendVarint(data, (static_cast<quint64>(fieldNumber) << 3U) | 2U);
    AppendVarint(data, static_cast<quint64>(value.size()));
    data->append(value);
}

void AppendVarintField(QByteArray *data, const quint32 fieldNumber, const quint64 value)
{
    AppendVarint(data, static_cast<quint64>(fieldNumber) << 3U);
    AppendVarint(data, value);
}

QByteArray MetadataEntry(const QString &key, const QString &value)
{
    QByteArray entry;
    AppendLengthDelimitedField(&entry, 1U, key.toUtf8());
    AppendLengthDelimitedField(&entry, 2U, value.toUtf8());
    return entry;
}

QString ClassNamesToMetadata(const QStringList &classNames)
{
    QStringList items;
    items.reserve(classNames.size());
    for (int index = 0; index < classNames.size(); ++index)
    {
        QString className = classNames.at(index);
        className.replace('\\', QStringLiteral("\\\\"));
        className.replace('\'', QStringLiteral("\\'"));
        items.append(QStringLiteral("%1: '%2'").arg(index).arg(className));
    }
    return QStringLiteral("{%1}").arg(items.join(QStringLiteral(", ")));
}

QString MetadataValue(const plugin_api::ModelExportConfig &config, const QString &key, const QString &defaultValue)
{
    const QString value = config.metadata.value(key).toString().trimmed();
    return value.isEmpty() ? defaultValue : value;
}

bool WriteYolo11OnnxFile(const std::string &serialized,
                         const plugin_api::ModelExportConfig &config,
                         const Yolo11CheckpointMetadata &checkpointMetadata,
                         QString *errorMessage)
{
    const QString description = config.metadata.value(QStringLiteral("description")).toString().trimmed();
    if (description.isEmpty())
    {
        *errorMessage = QString(u8"YOLO11 ONNX 元数据缺少 description");
        return false;
    }

    QByteArray modelData = QByteArray::fromStdString(serialized);
    AppendVarintField(&modelData, 1U, 7U);
    AppendLengthDelimitedField(&modelData, 2U, QByteArrayLiteral("pytorch"));
    AppendLengthDelimitedField(&modelData, 3U, QByteArrayLiteral("2.4.1"));
    AppendVarintField(&modelData, 5U, 0U);
    const std::array<std::pair<QString, QString>, 11> metadata{
        std::pair{QStringLiteral("author"),
                  MetadataValue(config, QStringLiteral("author"), QStringLiteral("YtVision/MHW"))},
        std::pair{QStringLiteral("license"),
                  MetadataValue(config, QStringLiteral("license"), QStringLiteral("AGPL-3.0 License"))},
        std::pair{QStringLiteral("description"), description},
        std::pair{QStringLiteral("date"),
                  QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz")) +
                      QStringLiteral("000")},
        std::pair{QStringLiteral("version"),
                  MetadataValue(config, QStringLiteral("version"), QStringLiteral("yolov11n v2.0"))},
        std::pair{QStringLiteral("docs"), MetadataValue(config, QStringLiteral("docs"), QStringLiteral("none"))},
        std::pair{QStringLiteral("stride"), QStringLiteral("32")},
        std::pair{QStringLiteral("task"), QStringLiteral("detect")},
        std::pair{QStringLiteral("batch"), QString::number(config.batchSize)},
        std::pair{QStringLiteral("imgsz"), QStringLiteral("[%1, %2]").arg(config.imageWidth).arg(config.imageHeight)},
        std::pair{QStringLiteral("names"), ClassNamesToMetadata(checkpointMetadata.classNames)}};
    for (const auto &[key, value] : metadata)
    {
        AppendLengthDelimitedField(&modelData, 14U, MetadataEntry(key, value));
    }

    const QFileInfo outputInfo(config.outputPath);
    if (!QDir().mkpath(outputInfo.dir().absolutePath()))
    {
        *errorMessage = QString(u8"无法创建 YOLO11 ONNX 输出目录: %1").arg(outputInfo.dir().absolutePath());
        return false;
    }
    QSaveFile outputFile(config.outputPath);
    if (!outputFile.open(QIODevice::WriteOnly))
    {
        *errorMessage = QString(u8"无法写入 YOLO11 ONNX 文件 %1: %2").arg(config.outputPath, outputFile.errorString());
        return false;
    }
    if (outputFile.write(modelData) != modelData.size() || !outputFile.commit())
    {
        *errorMessage = QString(u8"无法提交 YOLO11 ONNX 文件 %1: %2").arg(config.outputPath, outputFile.errorString());
        return false;
    }
    return true;
}

torch::Tensor DecodeYolo11ExportOutput(const std::vector<torch::Tensor> &outputs,
                                       const int classCount,
                                       const int imageWidth,
                                       const int imageHeight)
{
    constexpr std::array<int, 3> kStrides{8, 16, 32};
    const int outputChannels = kRegMax * 4 + classCount;
    std::vector<torch::Tensor> flattened;
    std::vector<float> anchorValues;
    std::vector<float> strideValues;
    flattened.reserve(outputs.size());

    for (size_t level = 0; level < kStrides.size(); ++level)
    {
        const int stride = kStrides.at(level);
        const int height = imageHeight / stride;
        const int width = imageWidth / stride;
        const torch::Tensor &output = outputs.at(level);
        if (!output.defined() || output.dim() != 4 || output.size(1) != outputChannels || output.size(2) != height ||
            output.size(3) != width)
        {
            throw std::runtime_error("YOLO11 raw detect output shape is invalid");
        }
        flattened.push_back(output.view({output.size(0), outputChannels, -1}));
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                anchorValues.push_back(static_cast<float>(x) + 0.5F);
                anchorValues.push_back(static_cast<float>(y) + 0.5F);
                strideValues.push_back(static_cast<float>(stride));
            }
        }
    }

    const int64_t anchorCount = static_cast<int64_t>(strideValues.size());
    const torch::Tensor values = torch::cat(flattened, 2);
    const torch::Tensor box = values.slice(1, 0, kRegMax * 4);
    const torch::Tensor scores = values.slice(1, kRegMax * 4);
    const torch::Tensor anchorPoints =
        torch::from_blob(anchorValues.data(), {anchorCount, 2}, torch::TensorOptions().dtype(torch::kFloat32))
            .clone()
            .transpose(0, 1)
            .unsqueeze(0);
    const torch::Tensor strides =
        torch::from_blob(strideValues.data(), {1, 1, anchorCount}, torch::TensorOptions().dtype(torch::kFloat32))
            .clone();
    const torch::Tensor projection =
        torch::arange(kRegMax, torch::TensorOptions().dtype(torch::kFloat32)).view({1, kRegMax, 1, 1});
    const torch::Tensor distance =
        (box.view({values.size(0), 4, kRegMax, anchorCount}).transpose(1, 2).softmax(1) * projection).sum(1);
    const torch::Tensor topLeft = anchorPoints - distance.slice(1, 0, 2);
    const torch::Tensor bottomRight = anchorPoints + distance.slice(1, 2, 4);
    const torch::Tensor boxes = torch::cat({(topLeft + bottomRight) / 2.0, bottomRight - topLeft}, 1) * strides;
    return torch::cat({boxes, scores.sigmoid()}, 1);
}

bool ExportYolo11Onnx(const plugin_api::ModelExportConfig &config, QString *errorMessage)
{
    if (config.format.compare(QStringLiteral("onnx"), Qt::CaseInsensitive) != 0 || config.checkpointPath.isEmpty() ||
        !QFileInfo::exists(config.checkpointPath) || config.outputPath.isEmpty() || config.batchSize <= 0 ||
        config.imageWidth <= 0 || config.imageHeight <= 0 || config.imageWidth % 32 != 0 ||
        config.imageHeight % 32 != 0)
    {
        *errorMessage = QString(u8"YOLO11 ONNX 导出参数无效");
        return false;
    }

    Yolo11CheckpointMetadata checkpointMetadata;
    if (!ReadYolo11CheckpointMetadata(config.checkpointPath, &checkpointMetadata, errorMessage))
    {
        return false;
    }
    if (checkpointMetadata.imageWidth != config.imageWidth || checkpointMetadata.imageHeight != config.imageHeight)
    {
        *errorMessage = QString(u8"YOLO11 checkpoint 输入尺寸与 ONNX 导出配置不匹配, checkpoint: %1x%2, 配置: %3x%4")
                            .arg(checkpointMetadata.imageWidth)
                            .arg(checkpointMetadata.imageHeight)
                            .arg(config.imageWidth)
                            .arg(config.imageHeight);
        return false;
    }

    try
    {
        Yolo11Network model(checkpointMetadata.classNames.size());
        torch::load(model, config.checkpointPath.toStdString(), torch::Device(torch::kCPU));
        model->eval();
        for (const auto &parameter : model->named_parameters(true))
        {
            parameter.value().set_requires_grad(false);
        }

        std::map<std::string, torch::Tensor> initializers;
        std::unordered_map<const c10::TensorImpl *, std::string> tensorNames;
        const auto addNamedTensors = [&initializers, &tensorNames](const auto &tensors)
        {
            for (const auto &tensor : tensors)
            {
                initializers.emplace(tensor.key(), tensor.value());
                tensorNames.emplace(tensor.value().unsafeGetTensorImpl(), tensor.key());
            }
        };
        addNamedTensors(model->named_parameters(true));
        addNamedTensors(model->named_buffers(true));

        const torch::Tensor input =
            torch::zeros({config.batchSize, kInputChannels, config.imageHeight, config.imageWidth},
                         torch::TensorOptions().dtype(torch::kFloat32));
        tensorNames.emplace(input.unsafeGetTensorImpl(), "images");
        torch::jit::Stack inputs;
        inputs.emplace_back(input);
        const auto trace = torch::jit::tracer::trace(
            std::move(inputs),
            [&model,
             classCount = checkpointMetadata.classNames.size(),
             imageWidth = config.imageWidth,
             imageHeight = config.imageHeight](torch::jit::Stack tracedInputs)
            {
                const std::vector<torch::Tensor> outputs = model->forward(tracedInputs.at(0).toTensor());
                return torch::jit::Stack{DecodeYolo11ExportOutput(outputs, classCount, imageWidth, imageHeight)};
            },
            [&tensorNames](const torch::autograd::Variable &variable)
            {
                const auto iterator = tensorNames.find(variable.unsafeGetTensorImpl());
                return iterator == tensorNames.cend() ? std::string() : iterator->second;
            },
            true,
            true,
            nullptr,
            {"images"});
        const auto exported = torch::jit::export_onnx(trace.first->graph,
                                                      initializers,
                                                      12,
                                                      {},
                                                      false,
                                                      torch::onnx::OperatorExportTypes::ONNX,
                                                      true,
                                                      false);
        const std::string serialized = torch::jit::serialize_model_proto_to_string(std::get<0>(exported));
        torch::jit::check_onnx_proto(serialized);
        return WriteYolo11OnnxFile(serialized, config, checkpointMetadata, errorMessage);
    }
    catch (const c10::Error &error)
    {
        *errorMessage = QString(u8"YOLO11 ONNX 导出失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return false;
    }
    catch (const std::exception &error)
    {
        *errorMessage = QString(u8"YOLO11 ONNX 导出失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return false;
    }
}
} // namespace

Yolo11Plugin::Yolo11Plugin() = default;

Yolo11Plugin::~Yolo11Plugin()
{
    stop();
    waitForStopped(-1);
}

plugin_api::PluginInfo Yolo11Plugin::pluginInfo() const
{
    return {QStringLiteral("visionaiflow.detection.yolov11"),
            QStringLiteral("YOLO11"),
            QStringLiteral("1.0.0"),
            plugin_api::TrainTaskType::Detection};
}

plugin_api::TrainState Yolo11Plugin::state() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

QString Yolo11Plugin::errorMessage() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_errorMessage;
}

bool Yolo11Plugin::stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == plugin_api::TrainState::Running)
    {
        m_stopRequested.store(true);
        m_state = plugin_api::TrainState::Stopping;
    }
    return true;
}

bool Yolo11Plugin::waitForStopped(const int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_finished)
    {
        if (timeoutMs < 0)
        {
            m_finishedCondition.wait(lock,
                                     [this]()
                                     {
                                         return m_finished;
                                     });
        }
        else if (!m_finishedCondition.wait_for(lock,
                                               std::chrono::milliseconds(timeoutMs),
                                               [this]()
                                               {
                                                   return m_finished;
                                               }))
        {
            return false;
        }
    }
    lock.unlock();
    if (m_worker.joinable())
    {
        m_worker.join();
    }
    return true;
}

bool Yolo11Plugin::initializeTraining(const plugin_api::DetectionTrainConfig &config)
{
    if (!waitForStopped(0))
    {
        setError(QString(u8"YOLO11 训练线程仍在运行, 无法重新初始化"));
        return false;
    }
    QString error;
    if (!m_trainer.initialize(config, &error))
    {
        setError(error);
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progress = {};
    m_metrics = {std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::quiet_NaN()};
    m_errorMessage.clear();
    m_stopRequested.store(false);
    m_finished = true;
    m_state = plugin_api::TrainState::Initialized;
    return true;
}

bool Yolo11Plugin::startTrain()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state != plugin_api::TrainState::Initialized)
        {
            m_errorMessage = QString(u8"YOLO11 训练插件尚未初始化");
            return false;
        }
        m_stopRequested.store(false);
        m_finished = false;
        m_state = plugin_api::TrainState::Running;
    }
    std::lock_guard<std::mutex> lock(m_inferenceMutex);
    m_inference.reset();
    m_worker = std::thread(&Yolo11Plugin::runTraining, this);
    return true;
}

plugin_api::DetectionTrainProgress Yolo11Plugin::progress() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_progress;
}

plugin_api::DetectionMetrics Yolo11Plugin::metrics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metrics;
}

bool Yolo11Plugin::loadInferenceModel(const plugin_api::DetectionInferConfig &config)
{
    if (state() == plugin_api::TrainState::Running || state() == plugin_api::TrainState::Stopping)
    {
        setError(QString(u8"YOLO11 训练运行时不能加载推理模型"));
        return false;
    }
    auto inference = std::make_unique<Yolo11Inference>();
    QString error;
    if (!inference->loadModel(config, &error))
    {
        setError(error);
        return false;
    }
    std::lock_guard<std::mutex> lock(m_inferenceMutex);
    m_inference = std::move(inference);
    return true;
}

bool Yolo11Plugin::infer(const plugin_api::DetectionInferRequest &request, plugin_api::DetectionInferResult *result)
{
    std::lock_guard<std::mutex> lock(m_inferenceMutex);
    if (m_inference == nullptr)
    {
        setError(QString(u8"YOLO11 推理模型尚未加载"));
        return false;
    }
    QString error;
    if (!m_inference->infer(request, result, &error))
    {
        setError(error);
        return false;
    }
    return true;
}

bool Yolo11Plugin::exportModel(const plugin_api::ModelExportConfig &config)
{
    if (state() == plugin_api::TrainState::Running || state() == plugin_api::TrainState::Stopping)
    {
        setError(QString(u8"YOLO11 训练运行时不能导出 ONNX"));
        return false;
    }
    QString error;
    if (!ExportYolo11Onnx(config, &error))
    {
        setError(error);
        return false;
    }
    return true;
}

bool Yolo11Plugin::exportBackbone(const plugin_api::BackboneExportConfig &config)
{
    Q_UNUSED(config)
    setError(QString(u8"YOLO11 插件不支持 Backbone 导出"));
    return false;
}

plugin_api::DetectionPluginCapabilities Yolo11Plugin::capabilities() const
{
    plugin_api::DetectionPluginCapabilities capabilities;
    capabilities.supportsPretrained = true;
    capabilities.supportsExport = true;
    return capabilities;
}

QVector<plugin_api::PluginParameterDefinition> Yolo11Plugin::parameterDefinitions() const
{
    return {
        {QStringLiteral("model_variant"), QString(u8"模型规格"), QStringLiteral("yolo11n"), {}, {}, QString(u8"模型")}};
}

void Yolo11Plugin::runTraining()
{
    QString error;
    const TrainRunResult result = m_trainer.train(
        m_stopRequested,
        [this](const plugin_api::DetectionTrainProgress &progress)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_progress = progress;
        },
        &error);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (result == TrainRunResult::Failed)
        {
            m_errorMessage = error.isEmpty() ? QString(u8"YOLO11 训练失败") : error;
            m_state = plugin_api::TrainState::Failed;
        }
        else
        {
            m_progress.train.message =
                result == TrainRunResult::Cancelled ? QStringLiteral("cancelled") : QStringLiteral("completed");
            m_state = plugin_api::TrainState::Completed;
        }
        m_finished = true;
    }
    m_finishedCondition.notify_all();
}

void Yolo11Plugin::setError(const QString &errorMessage)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorMessage = errorMessage;
    qWarning().noquote() << errorMessage;
}
} // namespace visionaiflow::yolov11
