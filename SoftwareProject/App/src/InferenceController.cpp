#include "visionaiflow/app/InferenceController.h"

#include "visionaiflow/models/yolo11/Yolo11DetectionDecoder.h"

#include <QImage>
#include <QPainter>
#include <QDebug>
#include <QThread>

#include <openvino/openvino.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace visionaiflow::app
{
namespace
{
struct LetterboxedImage final
{
    QVector<float> values;
    models::yolo11::LetterboxGeometry geometry;
};

LetterboxedImage CreateLetterboxedImage(const QString &imagePath, int networkWidth, int networkHeight)
{
    QImage source(imagePath);
    if (source.isNull())
    {
        throw std::runtime_error("Inference image could not be loaded: " + imagePath.toStdString());
    }

    const QImage rgb = source.convertToFormat(QImage::Format_RGB888);
    const auto geometry = models::yolo11::CreateYolo11LetterboxGeometry(
        static_cast<float>(rgb.width()),
        static_cast<float>(rgb.height()),
        static_cast<float>(networkWidth),
        static_cast<float>(networkHeight),
        true);
    if (!geometry.IsSuccess())
    {
        throw std::runtime_error(geometry.Failure().message);
    }

    QImage canvas(networkWidth, networkHeight, QImage::Format_RGB888);
    canvas.fill(QColor(114, 114, 114));
    const QSize scaledSize(
        static_cast<int>(std::round(static_cast<double>(rgb.width()) * geometry.Value().scale)),
        static_cast<int>(std::round(static_cast<double>(rgb.height()) * geometry.Value().scale)));
    QPainter painter(&canvas);
    painter.drawImage(
        static_cast<int>(std::round(geometry.Value().padX)),
        static_cast<int>(std::round(geometry.Value().padY)),
        rgb.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    painter.end();

    QVector<float> values(3 * networkWidth * networkHeight);
    for (int y = 0; y < networkHeight; ++y)
    {
        const auto *row = canvas.constScanLine(y);
        for (int x = 0; x < networkWidth; ++x)
        {
            const int inputOffset = x * 3;
            const int pixelOffset = y * networkWidth + x;
            values[pixelOffset] = static_cast<float>(row[inputOffset]) / 255.0F;
            values[networkWidth * networkHeight + pixelOffset] = static_cast<float>(row[inputOffset + 1]) / 255.0F;
            values[2 * networkWidth * networkHeight + pixelOffset] = static_cast<float>(row[inputOffset + 2]) / 255.0F;
        }
    }

    return {std::move(values), geometry.Value()};
}

QVector<InferenceDetection> RunInference(const InferenceRequest &request)
{
    if (request.modelPath.isEmpty() || request.imagePath.isEmpty() || request.classCount <= 0)
    {
        throw std::runtime_error("Inference requires a valid ONNX model, project image and class count");
    }

    ov::Core core;
    const std::shared_ptr<ov::Model> model = core.read_model(request.modelPath.toUtf8().constData());
    if (!model)
    {
        throw std::runtime_error("OpenVINO returned no ONNX model");
    }

    ov::CompiledModel compiled = core.compile_model(
        model,
        "CPU",
        ov::hint::performance_mode(ov::hint::PerformanceMode::LATENCY),
        ov::hint::inference_precision(ov::element::f32));
    if (compiled.inputs().size() != 1U || compiled.outputs().size() != 1U)
    {
        throw std::runtime_error("YOLO11 ONNX model must have one input and one output");
    }

    const ov::Output<const ov::Node> inputPort = compiled.input();
    const ov::PartialShape inputShape = inputPort.get_partial_shape();
    if (inputPort.get_element_type() != ov::element::f32
        || inputShape.rank().is_dynamic()
        || inputShape.rank().get_length() != 4
        || !inputShape[0].is_static()
        || !inputShape[1].is_static()
        || !inputShape[2].is_static()
        || !inputShape[3].is_static()
        || inputShape[0].get_length() != 1
        || inputShape[1].get_length() != 3)
    {
        throw std::runtime_error("YOLO11 ONNX input must be static [1,3,H,W] float32");
    }

    const int networkHeight = static_cast<int>(inputShape[2].get_length());
    const int networkWidth = static_cast<int>(inputShape[3].get_length());
    const LetterboxedImage image = CreateLetterboxedImage(request.imagePath, networkWidth, networkHeight);
    ov::InferRequest inferRequest = compiled.create_infer_request();
    ov::Tensor inputTensor(
        ov::element::f32,
        ov::Shape{1U, 3U, static_cast<size_t>(networkHeight), static_cast<size_t>(networkWidth)});
    float *inputData = inputTensor.data<float>();
    if (inputData == nullptr)
    {
        throw std::runtime_error("OpenVINO could not provide writable input memory");
    }
    std::copy(image.values.cbegin(), image.values.cend(), inputData);
    inferRequest.set_input_tensor(inputTensor);
    inferRequest.infer();

    const ov::Tensor outputTensor = inferRequest.get_output_tensor();
    const ov::Shape outputShape = outputTensor.get_shape();
    if (outputTensor.get_element_type() != ov::element::f32
        || outputShape.size() != 3U
        || outputShape[0] != 1U
        || outputShape[1] == 0U
        || outputShape[2] != static_cast<size_t>(4 + request.classCount))
    {
        throw std::runtime_error("YOLO11 ONNX output must be [1,rows,4+classCount] float32");
    }

    const float *outputData = static_cast<const float *>(outputTensor.data());
    if (outputData == nullptr)
    {
        throw std::runtime_error("OpenVINO could not read inference output");
    }
    const std::vector<float> rawOutput(outputData, outputData + outputTensor.get_size());
    const auto decoded = models::yolo11::DecodeYolo11DetectionsFromLetterbox(
        rawOutput,
        static_cast<int>(outputShape[1]),
        request.classCount,
        image.geometry,
        {});
    if (!decoded.IsSuccess())
    {
        throw std::runtime_error(decoded.Failure().message);
    }

    QVector<InferenceDetection> detections;
    detections.reserve(static_cast<qsizetype>(decoded.Value().size()));
    for (const auto &detection : decoded.Value())
    {
        detections.append(
            {detection.classIndex,
             detection.score,
             detection.box.x1,
             detection.box.y1,
             detection.box.x2,
             detection.box.y2});
    }
    return detections;
}
}

InferenceController::InferenceController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<InferenceDetection>();
    qRegisterMetaType<QVector<InferenceDetection>>();
}

InferenceController::~InferenceController()
{
    if (m_thread != nullptr)
    {
        m_thread->wait();
    }
}

void InferenceController::Start(const InferenceRequest &request)
{
    if (IsRunning())
    {
        emit Failed(QString(u8"推理任务正在运行"));
        return;
    }

    auto *thread = QThread::create([this, request]()
    {
        try
        {
            emit Completed(RunInference(request));
        }
        catch (const ov::Exception &error)
        {
            const QString message = QString(u8"OpenVINO推理失败: %1").arg(QString::fromLocal8Bit(error.what()));
            qCritical().noquote() << message;
            emit Failed(message);
        }
        catch (const std::exception &error)
        {
            const QString message = QString::fromLocal8Bit(error.what());
            qCritical().noquote() << QString(u8"yolodet推理失败:") << message;
            emit Failed(message);
        }
        catch (...)
        {
            const QString message = QString(u8"yolodet推理发生未知异常");
            qCritical().noquote() << message;
            emit Failed(message);
        }
    });
    m_thread = thread;
    connect(thread, &QThread::finished, this, [this, thread]()
    {
        thread->deleteLater();
        m_thread = nullptr;
        emit StateChanged(false);
    });
    emit StateChanged(true);
    thread->start();
}

bool InferenceController::IsRunning() const noexcept
{
    return m_thread != nullptr && m_thread->isRunning();
}
}
