#include "visionaiflow/training/Yolo11DetectionTraining.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

namespace visionaiflow::training
{
namespace
{
bool IsValidBox(const models::common::DetectionBox &box)
{
    return std::isfinite(box.x1) && std::isfinite(box.y1) && std::isfinite(box.x2) && std::isfinite(box.y2) && box.x2 > box.x1 && box.y2 > box.y1;
}

foundation::Result<void> ValidateAssignmentInputs(const std::vector<models::common::DetectionBox> &candidateBoxes, const std::vector<Yolo11GroundTruthDetection> &groundTruth, const int classCount, const Yolo11AssignmentConfig &config)
{
    if (classCount <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 detection classCount must be positive"));
    if (!std::isfinite(config.positiveIouThreshold) || config.positiveIouThreshold < 0.0F || config.positiveIouThreshold > 1.0F) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 assignment IoU threshold must be finite and in the closed interval from zero to one"));
    for (const auto &box : candidateBoxes)
    {
        if (!IsValidBox(box)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 candidate boxes must have positive finite area"));
    }
    for (const auto &target : groundTruth)
    {
        if (!IsValidBox(target.box)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 ground truth boxes must have positive finite area"));
        if (target.classIndex < 0 || target.classIndex >= classCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 ground truth class index is outside classCount"));
    }
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidateGroundTruthTargets(const std::vector<Yolo11GroundTruthDetection> &groundTruth, const int classCount)
{
    if (classCount <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 detection classCount must be positive"));
    for (const auto &target : groundTruth)
    {
        if (!IsValidBox(target.box)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 ground truth boxes must have positive finite area"));
        if (target.classIndex < 0 || target.classIndex >= classCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 ground truth class index is outside classCount"));
    }
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidateLossInputs(const torch::Tensor &rawHead, const std::vector<Yolo11AssignedTarget> &assignments, const int classCount, const Yolo11DetectionLossConfig &config)
{
    if (classCount <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 detection classCount must be positive"));
    if (!rawHead.defined()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 raw head tensor must be defined"));
    if (rawHead.dim() != 2 || rawHead.size(0) < 0 || rawHead.size(1) != 4 + classCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 raw head tensor must have shape [rows, 4 + classCount]"));
    if (rawHead.scalar_type() != torch::kFloat32) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 raw head tensor must be float32"));
    if (assignments.size() != static_cast<size_t>(rawHead.size(0))) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 assignment count must match raw head rows"));
    if (!std::isfinite(config.boxWeight) || !std::isfinite(config.classWeight) || config.boxWeight < 0.0 || config.classWeight < 0.0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 loss weights must be non-negative finite values"));
    for (const Yolo11AssignedTarget &assignment : assignments)
    {
        if (!assignment.positive) continue;
        if (assignment.classIndex < 0 || assignment.classIndex >= classCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 positive assignment class index is outside classCount"));
        if (!IsValidBox(assignment.box)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 positive assignment box must have positive finite area"));
    }
    return foundation::Result<void>::Success();
}

torch::Tensor BoxTensorFromAssignments(const std::vector<Yolo11AssignedTarget> &assignments, const std::vector<int64_t> &positiveRows, const torch::TensorOptions &options)
{
    std::vector<float> values;
    values.reserve(positiveRows.size() * 4U);
    for (const int64_t row : positiveRows)
    {
        const Yolo11AssignedTarget &target = assignments.at(static_cast<size_t>(row));
        values.push_back(target.box.x1);
        values.push_back(target.box.y1);
        values.push_back(target.box.x2);
        values.push_back(target.box.y2);
    }
    return torch::from_blob(values.data(), {static_cast<int64_t>(positiveRows.size()), 4}, torch::TensorOptions().dtype(torch::kFloat32)).clone().to(options.device());
}

torch::Tensor ClassTargetTensor(const std::vector<Yolo11AssignedTarget> &assignments, const int64_t rowCount, const int classCount, const torch::TensorOptions &options)
{
    torch::Tensor target = torch::zeros({rowCount, classCount}, options);
    for (int64_t row = 0; row < rowCount; ++row)
    {
        const Yolo11AssignedTarget &assignment = assignments.at(static_cast<size_t>(row));
        if (assignment.positive) target.index_put_({row, assignment.classIndex}, 1.0F);
    }
    return target;
}

QString CheckpointManifestPath(const QString &path)
{
    return path + QStringLiteral(".manifest.json");
}

QString CheckpointHashPath(const QString &path)
{
    return path + QStringLiteral(".sha256");
}

foundation::Result<QByteArray> Sha256Bytes(const QByteArray &bytes)
{
    if (bytes.isEmpty()) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint bytes are empty and cannot be hashed"));
    return foundation::Result<QByteArray>::Success(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

foundation::Result<void> WriteTextAtomically(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to open checkpoint metadata output: ").append(file.errorString()).toStdString()));
    if (file.write(bytes) != bytes.size() || !file.commit()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to atomically write checkpoint metadata: ").append(file.errorString()).toStdString()));
    return foundation::Result<void>::Success();
}

foundation::Result<QJsonObject> ReadJsonObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read checkpoint manifest: ").append(file.errorString()).toStdString()));
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest JSON is invalid or not an object"));
    return foundation::Result<QJsonObject>::Success(document.object());
}

QJsonArray ParameterNamesToJson(const QStringList &parameterNames)
{
    QJsonArray values;
    for (const QString &name : parameterNames) values.append(name);
    return values;
}

std::vector<int64_t> TensorShape(const torch::Tensor &tensor)
{
    std::vector<int64_t> shape;
    shape.reserve(static_cast<size_t>(tensor.dim()));
    for (const int64_t dimension : tensor.sizes()) shape.push_back(dimension);
    return shape;
}

foundation::Result<std::vector<int64_t>> ParameterShapeByName(const torch::nn::Module &model, const QString &name)
{
    for (const auto &item : model.named_parameters())
    {
        if (QString::fromStdString(item.key()) == name) return foundation::Result<std::vector<int64_t>>::Success(TensorShape(item.value()));
    }
    return foundation::Result<std::vector<int64_t>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "YOLO11 model parameter is missing from the runtime module"));
}

foundation::Result<QJsonArray> ParameterShapesToJson(const torch::nn::Module &model, const QStringList &expectedNames)
{
    QJsonArray values;
    for (const QString &name : expectedNames)
    {
        const auto shapeResult = ParameterShapeByName(model, name);
        if (!shapeResult.IsSuccess()) return foundation::Result<QJsonArray>::Failure(shapeResult.Failure());
        QJsonArray shape;
        for (const int64_t dimension : shapeResult.Value()) shape.append(static_cast<double>(dimension));
        values.append(QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("shape"), shape}});
    }
    return foundation::Result<QJsonArray>::Success(values);
}

foundation::Result<QStringList> ExpectedParameterNamesForSchema(const char *schemaName)
{
    const QString schema = QString::fromLatin1(schemaName);
    if (schema == QStringLiteral("yolo11_tiny_detector")) return foundation::Result<QStringList>::Success(Yolo11TinyDetectorParameterNames());
    if (schema == QStringLiteral("yolo11_grid_detector")) return foundation::Result<QStringList>::Success(Yolo11GridDetectorParameterNames());
    return foundation::Result<QStringList>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 checkpoint schema does not have a parameter name contract"));
}

foundation::Result<void> ValidateManifestParameterNames(const QJsonObject &manifest, const QStringList &expectedNames)
{
    const QJsonValue value = manifest.value(QStringLiteral("parameterNames"));
    if (!value.isArray()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterNames must be an array"));
    const QJsonArray actual = value.toArray();
    if (actual.size() != expectedNames.size()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterNames count does not match the YOLO11 model contract"));
    for (qsizetype index = 0; index < expectedNames.size(); ++index)
    {
        if (!actual.at(index).isString() || actual.at(index).toString() != expectedNames.at(index)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterNames do not match the YOLO11 model contract"));
    }
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidateManifestParameterShapes(const QJsonObject &manifest, const QStringList &expectedNames, const torch::nn::Module &model)
{
    const QJsonValue value = manifest.value(QStringLiteral("parameterShapes"));
    if (!value.isArray()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterShapes must be an array"));
    const QJsonArray actual = value.toArray();
    if (actual.size() != expectedNames.size()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterShapes count does not match the YOLO11 model contract"));
    for (qsizetype index = 0; index < expectedNames.size(); ++index)
    {
        if (!actual.at(index).isObject()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterShapes entries must be objects"));
        const QJsonObject entry = actual.at(index).toObject();
        if (entry.value(QStringLiteral("name")).toString() != expectedNames.at(index)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterShapes names do not match the YOLO11 model contract"));
        const QJsonValue shapeValue = entry.value(QStringLiteral("shape"));
        if (!shapeValue.isArray()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterShapes shape must be an array"));
        const QJsonArray manifestShape = shapeValue.toArray();
        const auto runtimeShape = ParameterShapeByName(model, expectedNames.at(index));
        if (!runtimeShape.IsSuccess()) return foundation::Result<void>::Failure(runtimeShape.Failure());
        if (manifestShape.size() != static_cast<qsizetype>(runtimeShape.Value().size())) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameter shape rank does not match the runtime model"));
        for (qsizetype dimensionIndex = 0; dimensionIndex < manifestShape.size(); ++dimensionIndex)
        {
            if (!manifestShape.at(dimensionIndex).isDouble()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameter shape dimensions must be numbers"));
            const double manifestDimension = manifestShape.at(dimensionIndex).toDouble(-1.0);
            if (!std::isfinite(manifestDimension) || std::floor(manifestDimension) != manifestDimension) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameter shape dimensions must be finite integers"));
            if (static_cast<int64_t>(manifestDimension) != runtimeShape.Value().at(static_cast<size_t>(dimensionIndex))) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameter shape does not match the runtime model"));
        }
    }
    return foundation::Result<void>::Success();
}

foundation::Result<void> WriteCheckpointMetadata(const QString &path, const QByteArray &bytes, const torch::nn::Module &model, const char *schemaName, const TrainingCheckpointState &state)
{
    const auto hash = Sha256Bytes(bytes);
    if (!hash.IsSuccess()) return foundation::Result<void>::Failure(hash.Failure());
    const auto expectedParameterNames = ExpectedParameterNamesForSchema(schemaName);
    if (!expectedParameterNames.IsSuccess()) return foundation::Result<void>::Failure(expectedParameterNames.Failure());
    const auto parameterShapes = ParameterShapesToJson(model, expectedParameterNames.Value());
    if (!parameterShapes.IsSuccess()) return foundation::Result<void>::Failure(parameterShapes.Failure());
    const auto trainingState = TrainingCheckpointStateToJson(state);
    if (!trainingState.IsSuccess()) return foundation::Result<void>::Failure(trainingState.Failure());
    const QFileInfo fileInfo(path);
    const QJsonObject manifest{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("schemaName"), QString::fromLatin1(schemaName)},
        {QStringLiteral("archiveFile"), fileInfo.fileName()},
        {QStringLiteral("archiveBytes"), bytes.size()},
        {QStringLiteral("archiveSha256"), QString::fromLatin1(hash.Value())},
        {QStringLiteral("productId"), QStringLiteral("VisionAIFlowV1")},
        {QStringLiteral("adapterId"), QStringLiteral("visionaiflow.yolo11")},
        {QStringLiteral("adapterVersion"), QStringLiteral("0.1.0")},
        {QStringLiteral("libtorchVersion"), QString::fromLatin1(TORCH_VERSION)},
        {QStringLiteral("parameterNames"), ParameterNamesToJson(expectedParameterNames.Value())},
        {QStringLiteral("parameterShapes"), parameterShapes.Value()},
        {QStringLiteral("trainingState"), trainingState.Value()}};
    const QByteArray manifestBytes = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
    const auto manifestWritten = WriteTextAtomically(CheckpointManifestPath(path), manifestBytes);
    if (!manifestWritten.IsSuccess()) return manifestWritten;
    const QByteArray hashBytes = hash.Value() + QByteArrayLiteral("  ") + fileInfo.fileName().toUtf8() + QByteArrayLiteral("\n");
    return WriteTextAtomically(CheckpointHashPath(path), hashBytes);
}

foundation::Result<TrainingCheckpointState> VerifyCheckpointMetadata(const QString &path, const QByteArray &bytes, const torch::nn::Module &model, const char *schemaName)
{
    const auto manifestResult = ReadJsonObject(CheckpointManifestPath(path));
    if (!manifestResult.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(manifestResult.Failure());
    const QJsonObject manifest = manifestResult.Value();
    if (manifest.value(QStringLiteral("schemaVersion")).toInt() != 1) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest schema version is unsupported"));
    if (manifest.value(QStringLiteral("schemaName")).toString() != QString::fromLatin1(schemaName)) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest schema name does not match the requested YOLO11 detector"));
    if (manifest.value(QStringLiteral("archiveFile")).toString() != QFileInfo(path).fileName()) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest archive file does not match checkpoint path"));
    if (static_cast<qint64>(manifest.value(QStringLiteral("archiveBytes")).toDouble(-1.0)) != bytes.size()) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest byte count does not match checkpoint file"));
    if (manifest.value(QStringLiteral("productId")).toString() != QStringLiteral("VisionAIFlowV1")) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest product id is unsupported"));
    if (manifest.value(QStringLiteral("adapterId")).toString() != QStringLiteral("visionaiflow.yolo11")) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest adapter id is unsupported"));
    if (manifest.value(QStringLiteral("libtorchVersion")).toString() != QString::fromLatin1(TORCH_VERSION)) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest LibTorch version does not match this build"));
    const auto expectedParameterNames = ExpectedParameterNamesForSchema(schemaName);
    if (!expectedParameterNames.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(expectedParameterNames.Failure());
    const auto parameterNames = ValidateManifestParameterNames(manifest, expectedParameterNames.Value());
    if (!parameterNames.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(parameterNames.Failure());
    const auto parameterShapes = ValidateManifestParameterShapes(manifest, expectedParameterNames.Value(), model);
    if (!parameterShapes.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(parameterShapes.Failure());
    const auto trainingState = TrainingCheckpointStateFromJson(manifest.value(QStringLiteral("trainingState")).toObject());
    if (!trainingState.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(trainingState.Failure());
    const QString expectedHash = manifest.value(QStringLiteral("archiveSha256")).toString();
    if (expectedHash.size() != 64) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest hash is missing or invalid"));
    QFile hashFile(CheckpointHashPath(path));
    if (!hashFile.open(QIODevice::ReadOnly)) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read checkpoint hash file: ").append(hashFile.errorString()).toStdString()));
    const QString storedHashLine = QString::fromLatin1(hashFile.readAll()).trimmed();
    const QString storedHash = storedHashLine.section(QChar(' '), 0, 0);
    if (storedHash.compare(expectedHash, Qt::CaseInsensitive) != 0) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint hash file does not match manifest"));
    const auto actualHash = Sha256Bytes(bytes);
    if (!actualHash.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(actualHash.Failure());
    if (QString::fromLatin1(actualHash.Value()).compare(expectedHash, Qt::CaseInsensitive) != 0) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint checksum mismatch"));
    return foundation::Result<TrainingCheckpointState>::Success(trainingState.Value());
}

foundation::Result<void> ValidateDetectionBatchInputs(const Yolo11DetectionBatch &batch, const int classCount)
{
    if (classCount <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 detection classCount must be positive"));
    if (!batch.images.defined()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 detection batch images must be defined"));
    if (batch.images.dim() != 4 || batch.images.size(0) <= 0 || batch.images.size(1) <= 0 || batch.images.size(2) <= 0 || batch.images.size(3) <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 detection batch images must have shape [batch, channels, height, width]"));
    if (batch.images.scalar_type() != torch::kFloat32) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 detection batch images must be float32"));
    if (!torch::isfinite(batch.images).all().item<bool>()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 detection batch images contain a non-finite value"));
    if (batch.assignments.size() != static_cast<size_t>(batch.images.size(0))) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 detection assignment batch size must match image batch size"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidateDetectionBatch(Yolo11TinyDetector &model, const Yolo11DetectionBatch &batch, const int classCount)
{
    if (!model) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 tiny detector model must not be null"));
    return ValidateDetectionBatchInputs(batch, classCount);
}

foundation::Result<void> ValidateGridDetectionBatch(Yolo11GridDetector &model, const Yolo11DetectionBatch &batch, const int classCount)
{
    if (!model) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 grid detector model must not be null"));
    if (model->ClassCount() != classCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 grid detector classCount does not match the training contract"));
    const auto validation = ValidateDetectionBatchInputs(batch, classCount);
    if (!validation.IsSuccess()) return validation;
    if (batch.images.size(2) % model->OutputStride() != 0 || batch.images.size(3) % model->OutputStride() != 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 grid detector image height and width must be divisible by the output stride"));
    return foundation::Result<void>::Success();
}

foundation::Result<Yolo11DetectionBatchMetrics> SummarizeDetectionBatchLoss(const torch::Tensor &rawBatch, const std::vector<std::vector<Yolo11AssignedTarget>> &assignments, const int classCount, const Yolo11DetectionLossConfig &config)
{
    if (rawBatch.dim() != 3 || rawBatch.size(0) != static_cast<int64_t>(assignments.size()) || rawBatch.size(2) != 4 + classCount) return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "YOLO11 detector output shape does not match assignment and class contracts"));
    torch::Tensor accumulatedLoss = torch::zeros({}, rawBatch.options());
    double boxLoss = 0.0;
    double classLoss = 0.0;
    double iouWeightedSum = 0.0;
    int positiveRows = 0;
    int assignedGroundTruthCount = 0;
    for (int64_t sample = 0; sample < rawBatch.size(0); ++sample)
    {
        const auto loss = ComputeYolo11DetectionLoss(rawBatch.select(0, sample), assignments.at(static_cast<size_t>(sample)), classCount, config);
        if (!loss.IsSuccess()) return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(loss.Failure());
        accumulatedLoss = accumulatedLoss + loss.Value().totalLoss;
        boxLoss += loss.Value().boxLoss;
        classLoss += loss.Value().classLoss;
        iouWeightedSum += loss.Value().meanPositiveIou * static_cast<double>(loss.Value().positiveRows);
        positiveRows += loss.Value().positiveRows;
        assignedGroundTruthCount += loss.Value().assignedGroundTruthCount;
    }
    const double sampleCount = static_cast<double>(rawBatch.size(0));
    torch::Tensor meanLoss = accumulatedLoss / sampleCount;
    if (!torch::isfinite(meanLoss).all().item<bool>()) return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "YOLO11 detection batch loss is NaN or infinite"));
    const double meanIou = positiveRows > 0 ? iouWeightedSum / static_cast<double>(positiveRows) : 0.0;
    return foundation::Result<Yolo11DetectionBatchMetrics>::Success({meanLoss, boxLoss / sampleCount, classLoss / sampleCount, positiveRows, assignedGroundTruthCount, meanIou, static_cast<int>(rawBatch.size(0))});
}

foundation::Result<void> SaveCheckpoint(const QString &path, torch::nn::Module &model, torch::optim::Optimizer &optimizer, const char *schemaName, const TrainingCheckpointState &state)
{
    if (path.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint path must not be empty"));
    const QFileInfo fileInfo(path);
    if (!fileInfo.dir().exists()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint parent directory does not exist"));
    const auto stateValidation = ValidateTrainingCheckpointState(state);
    if (!stateValidation.IsSuccess()) return stateValidation;
    try
    {
        torch::serialize::OutputArchive root;
        torch::serialize::OutputArchive modelArchive;
        torch::serialize::OutputArchive optimizerArchive;
        model.save(modelArchive);
        optimizer.save(optimizerArchive);
        root.write("schemaVersion", static_cast<int64_t>(1));
        root.write("schemaName", std::string(schemaName));
        root.write("model", modelArchive);
        root.write("optimizer", optimizerArchive);
        const auto stateWritten = WriteTrainingCheckpointStateArchive(root, state);
        if (!stateWritten.IsSuccess()) return stateWritten;
        QByteArray bytes;
        bool serializationOverflow = false;
        root.save_to([&bytes, &serializationOverflow](const void *data, const size_t count) {
            if (count > static_cast<size_t>(std::numeric_limits<qsizetype>::max())) { serializationOverflow = true; return static_cast<size_t>(0); }
            bytes.append(static_cast<const char *>(data), static_cast<qsizetype>(count));
            return count;
        });
        if (serializationOverflow || bytes.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Checkpoint serialization produced invalid output"));
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to open checkpoint for writing: ").append(file.errorString()).toStdString()));
        if (file.write(bytes) != bytes.size() || !file.commit()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to atomically commit checkpoint: ").append(file.errorString()).toStdString()));
        return WriteCheckpointMetadata(path, bytes, model, schemaName, state);
    }
    catch (const c10::Error &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch checkpoint save failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, std::string("Checkpoint save failed: ") + error.what())); }
}

foundation::Result<void> LoadCheckpoint(const QString &path, torch::nn::Module &model, torch::optim::Optimizer &optimizer, const torch::Device &device, const char *schemaName, TrainingCheckpointState &state, const TrainingCheckpointLoadOptions &options)
{
    if (path.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint path must not be empty"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to open checkpoint for reading: ").append(file.errorString()).toStdString()));
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint file is empty"));
    const auto metadataVerified = VerifyCheckpointMetadata(path, bytes, model, schemaName);
    if (!metadataVerified.IsSuccess()) return foundation::Result<void>::Failure(metadataVerified.Failure());
    try
    {
        torch::serialize::InputArchive root;
        root.load_from(bytes.constData(), static_cast<size_t>(bytes.size()), device);
        c10::IValue schemaVersion;
        c10::IValue storedSchemaName;
        root.read("schemaVersion", schemaVersion);
        root.read("schemaName", storedSchemaName);
        if (!schemaVersion.isInt() || schemaVersion.toInt() != 1) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint schema version is unsupported"));
        if (!storedSchemaName.isString() || storedSchemaName.toStringRef() != schemaName) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint schema name does not match YOLO11 tiny detector"));
        torch::serialize::InputArchive modelArchive;
        torch::serialize::InputArchive optimizerArchive;
        root.read("model", modelArchive);
        root.read("optimizer", optimizerArchive);
        const auto archiveState = ReadTrainingCheckpointStateArchive(root, options.restoreCpuRng);
        if (!archiveState.IsSuccess()) return foundation::Result<void>::Failure(archiveState.Failure());
        const auto stateMatchesManifest = ValidateTrainingCheckpointStateMatch(metadataVerified.Value(), archiveState.Value());
        if (!stateMatchesManifest.IsSuccess()) return stateMatchesManifest;
        model.load(modelArchive);
        optimizer.load(optimizerArchive);
        if (archiveState.Value().captureCudaRng)
        {
            if (!options.restoreCudaRngStates) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Checkpoint contains CUDA RNG state but no CUDA RNG restore handler was provided"));
            const auto cudaRestored = options.restoreCudaRngStates(archiveState.Value().cudaRngStates);
            if (!cudaRestored.IsSuccess()) return cudaRestored;
        }
        state = archiveState.Value();
        return foundation::Result<void>::Success();
    }
    catch (const c10::Error &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("LibTorch checkpoint load failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("Checkpoint load failed: ") + error.what())); }
}
}

Yolo11TinyDetectorImpl::Yolo11TinyDetectorImpl(const int inputChannels, const int rowCount, const int classCount)
    : m_rowCount(rowCount)
    , m_classCount(classCount)
{
    m_conv1 = register_module("conv1", torch::nn::Conv2d(torch::nn::Conv2dOptions(inputChannels, 16, 3).stride(1).padding(1)));
    m_conv2 = register_module("conv2", torch::nn::Conv2d(torch::nn::Conv2dOptions(16, 32, 3).stride(2).padding(1)));
    m_head = register_module("head", torch::nn::Linear(32, rowCount * (4 + classCount)));
}

torch::Tensor Yolo11TinyDetectorImpl::forward(const torch::Tensor &images)
{
    torch::Tensor x = torch::relu(m_conv1->forward(images));
    x = torch::relu(m_conv2->forward(x));
    x = x.mean({2, 3});
    x = m_head->forward(x);
    return x.view({images.size(0), m_rowCount, 4 + m_classCount});
}

torch::Tensor Yolo11TinyDetectorImpl::Conv1Weight() const { return m_conv1->weight; }
torch::Tensor Yolo11TinyDetectorImpl::Conv1Bias() const { return m_conv1->bias; }
torch::Tensor Yolo11TinyDetectorImpl::Conv2Weight() const { return m_conv2->weight; }
torch::Tensor Yolo11TinyDetectorImpl::Conv2Bias() const { return m_conv2->bias; }
torch::Tensor Yolo11TinyDetectorImpl::HeadWeight() const { return m_head->weight; }
torch::Tensor Yolo11TinyDetectorImpl::HeadBias() const { return m_head->bias; }
int Yolo11TinyDetectorImpl::RowCount() const noexcept { return m_rowCount; }
int Yolo11TinyDetectorImpl::ClassCount() const noexcept { return m_classCount; }

Yolo11GridDetectorImpl::Yolo11GridDetectorImpl(const int inputChannels, const int classCount)
    : m_classCount(classCount)
{
    m_backbone = register_module("backbone", torch::nn::Sequential(
        torch::nn::Conv2d(torch::nn::Conv2dOptions(inputChannels, 16, 3).stride(2).padding(1).bias(false)),
        torch::nn::BatchNorm2d(16),
        torch::nn::SiLU(),
        torch::nn::Conv2d(torch::nn::Conv2dOptions(16, 32, 3).stride(2).padding(1).bias(false)),
        torch::nn::BatchNorm2d(32),
        torch::nn::SiLU(),
        torch::nn::Conv2d(torch::nn::Conv2dOptions(32, 64, 3).stride(2).padding(1).bias(false)),
        torch::nn::BatchNorm2d(64),
        torch::nn::SiLU()));
    m_neck = register_module("neck", torch::nn::Sequential(
        torch::nn::Conv2d(torch::nn::Conv2dOptions(64, 64, 1).stride(1).padding(0).bias(false)),
        torch::nn::BatchNorm2d(64),
        torch::nn::SiLU(),
        torch::nn::Conv2d(torch::nn::Conv2dOptions(64, 64, 3).stride(1).padding(1).bias(false)),
        torch::nn::BatchNorm2d(64),
        torch::nn::SiLU()));
    m_head = register_module("head", torch::nn::Conv2d(torch::nn::Conv2dOptions(64, 4 + classCount, 1).stride(1).padding(0)));
}

torch::Tensor Yolo11GridDetectorImpl::forward(const torch::Tensor &images)
{
    torch::Tensor x = m_backbone->forward(images);
    x = m_neck->forward(x);
    x = m_head->forward(x);
    x = x.permute({0, 2, 3, 1}).contiguous();
    return x.view({x.size(0), x.size(1) * x.size(2), 4 + m_classCount});
}

int Yolo11GridDetectorImpl::ClassCount() const noexcept { return m_classCount; }
int Yolo11GridDetectorImpl::OutputStride() const noexcept { return 8; }

foundation::Result<std::vector<Yolo11AssignedTarget>> AssignYolo11DetectionTargets(const std::vector<models::common::DetectionBox> &candidateBoxes, const std::vector<Yolo11GroundTruthDetection> &groundTruth, const int classCount, const Yolo11AssignmentConfig &config)
{
    const auto validation = ValidateAssignmentInputs(candidateBoxes, groundTruth, classCount, config);
    if (!validation.IsSuccess()) return foundation::Result<std::vector<Yolo11AssignedTarget>>::Failure(validation.Failure());
    std::vector<Yolo11AssignedTarget> assignments(candidateBoxes.size());
    if (candidateBoxes.empty() || groundTruth.empty()) return foundation::Result<std::vector<Yolo11AssignedTarget>>::Success(std::move(assignments));
    for (size_t row = 0; row < candidateBoxes.size(); ++row)
    {
        float bestIou = -1.0F;
        int bestIndex = -1;
        for (size_t targetIndex = 0; targetIndex < groundTruth.size(); ++targetIndex)
        {
            const float iou = models::common::IntersectionOverUnion(candidateBoxes[row], groundTruth[targetIndex].box);
            if (iou > bestIou)
            {
                bestIou = iou;
                bestIndex = static_cast<int>(targetIndex);
            }
        }
        if (bestIndex >= 0 && bestIou >= config.positiveIouThreshold)
        {
            assignments[row] = {true, bestIndex, groundTruth[static_cast<size_t>(bestIndex)].classIndex, groundTruth[static_cast<size_t>(bestIndex)].box};
        }
    }
    if (config.forceBestGroundTruthMatch)
    {
        for (size_t targetIndex = 0; targetIndex < groundTruth.size(); ++targetIndex)
        {
            float bestIou = -1.0F;
            size_t bestRow = 0U;
            for (size_t row = 0; row < candidateBoxes.size(); ++row)
            {
                const float iou = models::common::IntersectionOverUnion(candidateBoxes[row], groundTruth[targetIndex].box);
                if (iou > bestIou)
                {
                    bestIou = iou;
                    bestRow = row;
                }
            }
            assignments[bestRow] = {true, static_cast<int>(targetIndex), groundTruth[targetIndex].classIndex, groundTruth[targetIndex].box};
        }
    }
    return foundation::Result<std::vector<Yolo11AssignedTarget>>::Success(std::move(assignments));
}

foundation::Result<Yolo11TinyDetector> CreateYolo11TinyDetector(const int inputChannels, const int rowCount, const int classCount)
{
    if (inputChannels <= 0 || rowCount <= 0 || classCount <= 0) return foundation::Result<Yolo11TinyDetector>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 tiny detector requires positive inputChannels, rowCount and classCount"));
    try { return foundation::Result<Yolo11TinyDetector>::Success(Yolo11TinyDetector(inputChannels, rowCount, classCount)); }
    catch (const c10::Error &error) { return foundation::Result<Yolo11TinyDetector>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch YOLO11 tiny detector construction failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<Yolo11TinyDetector>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("YOLO11 tiny detector construction failed: ") + error.what())); }
}

foundation::Result<Yolo11GridDetector> CreateYolo11GridDetector(const int inputChannels, const int classCount)
{
    if (inputChannels <= 0 || classCount <= 0) return foundation::Result<Yolo11GridDetector>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 grid detector requires positive inputChannels and classCount"));
    try { return foundation::Result<Yolo11GridDetector>::Success(Yolo11GridDetector(inputChannels, classCount)); }
    catch (const c10::Error &error) { return foundation::Result<Yolo11GridDetector>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch YOLO11 grid detector construction failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<Yolo11GridDetector>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("YOLO11 grid detector construction failed: ") + error.what())); }
}

QStringList Yolo11TinyDetectorParameterNames()
{
    return {QStringLiteral("conv1.weight"), QStringLiteral("conv1.bias"), QStringLiteral("conv2.weight"), QStringLiteral("conv2.bias"), QStringLiteral("head.weight"), QStringLiteral("head.bias")};
}

QStringList Yolo11GridDetectorParameterNames()
{
    return {
        QStringLiteral("backbone.0.weight"),
        QStringLiteral("backbone.1.weight"),
        QStringLiteral("backbone.1.bias"),
        QStringLiteral("backbone.3.weight"),
        QStringLiteral("backbone.4.weight"),
        QStringLiteral("backbone.4.bias"),
        QStringLiteral("backbone.6.weight"),
        QStringLiteral("backbone.7.weight"),
        QStringLiteral("backbone.7.bias"),
        QStringLiteral("neck.0.weight"),
        QStringLiteral("neck.1.weight"),
        QStringLiteral("neck.1.bias"),
        QStringLiteral("neck.3.weight"),
        QStringLiteral("neck.4.weight"),
        QStringLiteral("neck.4.bias"),
        QStringLiteral("head.weight"),
        QStringLiteral("head.bias")};
}

foundation::Result<void> ValidateFiniteGradients(const torch::nn::Module &model, const char *context)
{
    bool observedGradient = false;
    for (const auto &item : model.named_parameters())
    {
        const torch::Tensor gradient = item.value().grad();
        if (!gradient.defined()) continue;
        observedGradient = true;
        if (!torch::isfinite(gradient).all().item<bool>()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string(context) + " produced a NaN or infinite gradient for parameter " + item.key()));
    }
    if (!observedGradient) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string(context) + " produced no gradients"));
    return foundation::Result<void>::Success();
}

foundation::Result<Yolo11DetectionBatchMetrics> TrainYolo11TinyDetectionStep(Yolo11TinyDetector &model, torch::optim::Optimizer &optimizer, const Yolo11DetectionBatch &batch, const int classCount, const Yolo11DetectionLossConfig &config)
{
    const auto validation = ValidateDetectionBatch(model, batch, classCount);
    if (!validation.IsSuccess()) return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(validation.Failure());
    try
    {
        model->train();
        optimizer.zero_grad();
        const auto metrics = SummarizeDetectionBatchLoss(model->forward(batch.images), batch.assignments, classCount, config);
        if (!metrics.IsSuccess()) return metrics;
        metrics.Value().totalLoss.backward();
        const auto gradients = ValidateFiniteGradients(*model, "YOLO11 tiny detector training");
        if (!gradients.IsSuccess()) { optimizer.zero_grad(); return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(gradients.Failure()); }
        optimizer.step();
        return metrics;
    }
    catch (const c10::Error &error) { return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch YOLO11 tiny detector training failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("YOLO11 tiny detector training failed: ") + error.what())); }
}

foundation::Result<Yolo11DetectionBatchMetrics> EvaluateYolo11TinyDetectionBatch(Yolo11TinyDetector &model, const Yolo11DetectionBatch &batch, const int classCount, const Yolo11DetectionLossConfig &config)
{
    const auto validation = ValidateDetectionBatch(model, batch, classCount);
    if (!validation.IsSuccess()) return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(validation.Failure());
    try
    {
        torch::NoGradGuard guard;
        model->eval();
        return SummarizeDetectionBatchLoss(model->forward(batch.images), batch.assignments, classCount, config);
    }
    catch (const c10::Error &error) { return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch YOLO11 tiny detector evaluation failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("YOLO11 tiny detector evaluation failed: ") + error.what())); }
}

foundation::Result<Yolo11DetectionBatchMetrics> TrainYolo11GridDetectionStep(Yolo11GridDetector &model, torch::optim::Optimizer &optimizer, const Yolo11DetectionBatch &batch, const int classCount, const Yolo11DetectionLossConfig &config)
{
    const auto validation = ValidateGridDetectionBatch(model, batch, classCount);
    if (!validation.IsSuccess()) return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(validation.Failure());
    try
    {
        model->train();
        optimizer.zero_grad();
        const auto metrics = SummarizeDetectionBatchLoss(model->forward(batch.images), batch.assignments, classCount, config);
        if (!metrics.IsSuccess()) return metrics;
        metrics.Value().totalLoss.backward();
        const auto gradients = ValidateFiniteGradients(*model, "YOLO11 grid detector training");
        if (!gradients.IsSuccess()) { optimizer.zero_grad(); return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(gradients.Failure()); }
        optimizer.step();
        return metrics;
    }
    catch (const c10::Error &error) { return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch YOLO11 grid detector training failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("YOLO11 grid detector training failed: ") + error.what())); }
}

foundation::Result<Yolo11DetectionBatchMetrics> EvaluateYolo11GridDetectionBatch(Yolo11GridDetector &model, const Yolo11DetectionBatch &batch, const int classCount, const Yolo11DetectionLossConfig &config)
{
    const auto validation = ValidateGridDetectionBatch(model, batch, classCount);
    if (!validation.IsSuccess()) return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(validation.Failure());
    try
    {
        torch::NoGradGuard guard;
        model->eval();
        return SummarizeDetectionBatchLoss(model->forward(batch.images), batch.assignments, classCount, config);
    }
    catch (const c10::Error &error) { return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch YOLO11 grid detector evaluation failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<Yolo11DetectionBatchMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("YOLO11 grid detector evaluation failed: ") + error.what())); }
}

foundation::Result<void> SaveYolo11TinyDetectorCheckpoint(const QString &path, Yolo11TinyDetector &model, torch::optim::Optimizer &optimizer)
{
    return SaveYolo11TinyDetectorCheckpoint(path, model, optimizer, TrainingCheckpointState{});
}

foundation::Result<void> SaveYolo11TinyDetectorCheckpoint(const QString &path, Yolo11TinyDetector &model, torch::optim::Optimizer &optimizer, const TrainingCheckpointState &state)
{
    if (!model) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 tiny detector model must not be null"));
    return SaveCheckpoint(path, *model, optimizer, "yolo11_tiny_detector", state);
}

foundation::Result<void> LoadYolo11TinyDetectorCheckpoint(const QString &path, Yolo11TinyDetector &model, torch::optim::Optimizer &optimizer, const torch::Device &device)
{
    TrainingCheckpointState ignoredState;
    return LoadYolo11TinyDetectorCheckpoint(path, model, optimizer, device, ignoredState);
}

foundation::Result<void> LoadYolo11TinyDetectorCheckpoint(const QString &path, Yolo11TinyDetector &model, torch::optim::Optimizer &optimizer, const torch::Device &device, TrainingCheckpointState &state)
{
    return LoadYolo11TinyDetectorCheckpoint(path, model, optimizer, device, state, TrainingCheckpointLoadOptions{});
}

foundation::Result<void> LoadYolo11TinyDetectorCheckpoint(const QString &path, Yolo11TinyDetector &model, torch::optim::Optimizer &optimizer, const torch::Device &device, TrainingCheckpointState &state, const TrainingCheckpointLoadOptions &options)
{
    if (!model) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 tiny detector model must not be null"));
    return LoadCheckpoint(path, *model, optimizer, device, "yolo11_tiny_detector", state, options);
}

foundation::Result<void> SaveYolo11GridDetectorCheckpoint(const QString &path, Yolo11GridDetector &model, torch::optim::Optimizer &optimizer)
{
    return SaveYolo11GridDetectorCheckpoint(path, model, optimizer, TrainingCheckpointState{});
}

foundation::Result<void> SaveYolo11GridDetectorCheckpoint(const QString &path, Yolo11GridDetector &model, torch::optim::Optimizer &optimizer, const TrainingCheckpointState &state)
{
    if (!model) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 grid detector model must not be null"));
    return SaveCheckpoint(path, *model, optimizer, "yolo11_grid_detector", state);
}

foundation::Result<void> LoadYolo11GridDetectorCheckpoint(const QString &path, Yolo11GridDetector &model, torch::optim::Optimizer &optimizer, const torch::Device &device)
{
    TrainingCheckpointState ignoredState;
    return LoadYolo11GridDetectorCheckpoint(path, model, optimizer, device, ignoredState);
}

foundation::Result<void> LoadYolo11GridDetectorCheckpoint(const QString &path, Yolo11GridDetector &model, torch::optim::Optimizer &optimizer, const torch::Device &device, TrainingCheckpointState &state)
{
    return LoadYolo11GridDetectorCheckpoint(path, model, optimizer, device, state, TrainingCheckpointLoadOptions{});
}

foundation::Result<void> LoadYolo11GridDetectorCheckpoint(const QString &path, Yolo11GridDetector &model, torch::optim::Optimizer &optimizer, const torch::Device &device, TrainingCheckpointState &state, const TrainingCheckpointLoadOptions &options)
{
    if (!model) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 grid detector model must not be null"));
    return LoadCheckpoint(path, *model, optimizer, device, "yolo11_grid_detector", state, options);
}

foundation::Result<Yolo11DetectionLossMetrics> ComputeYolo11DetectionLoss(const torch::Tensor &rawHead, const std::vector<Yolo11AssignedTarget> &assignments, const int classCount, const Yolo11DetectionLossConfig &config)
{
    const auto validation = ValidateLossInputs(rawHead, assignments, classCount, config);
    if (!validation.IsSuccess()) return foundation::Result<Yolo11DetectionLossMetrics>::Failure(validation.Failure());
    try
    {
        if (!torch::isfinite(rawHead).all().item<bool>()) return foundation::Result<Yolo11DetectionLossMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 raw head tensor contains a non-finite value"));
        std::vector<int64_t> positiveRows;
        std::set<int> assignedGroundTruth;
        positiveRows.reserve(assignments.size());
        for (size_t row = 0; row < assignments.size(); ++row)
        {
            if (!assignments[row].positive) continue;
            positiveRows.push_back(static_cast<int64_t>(row));
            if (assignments[row].groundTruthIndex >= 0) assignedGroundTruth.insert(assignments[row].groundTruthIndex);
        }
        const torch::Tensor classTargets = ClassTargetTensor(assignments, rawHead.size(0), classCount, rawHead.options());
        const torch::Tensor classPrediction = rawHead.index({torch::indexing::Slice(), torch::indexing::Slice(4, 4 + classCount)});
        const torch::Tensor classLoss = torch::mse_loss(classPrediction, classTargets);
        torch::Tensor boxLoss = torch::zeros({}, rawHead.options());
        double meanIou = 0.0;
        if (!positiveRows.empty())
        {
            const torch::Tensor rowIndex = torch::from_blob(positiveRows.data(), {static_cast<int64_t>(positiveRows.size())}, torch::TensorOptions().dtype(torch::kInt64)).clone().to(rawHead.device());
            const torch::Tensor predictedBoxes = rawHead.index_select(0, rowIndex).index({torch::indexing::Slice(), torch::indexing::Slice(0, 4)});
            const torch::Tensor targetBoxes = BoxTensorFromAssignments(assignments, positiveRows, rawHead.options());
            boxLoss = torch::smooth_l1_loss(predictedBoxes, targetBoxes);
            double iouSum = 0.0;
            for (const int64_t row : positiveRows)
            {
                const auto &target = assignments.at(static_cast<size_t>(row));
                const models::common::DetectionBox predicted{rawHead.index({row, 0}).item<float>(), rawHead.index({row, 1}).item<float>(), rawHead.index({row, 2}).item<float>(), rawHead.index({row, 3}).item<float>()};
                iouSum += models::common::IntersectionOverUnion(predicted, target.box);
            }
            meanIou = iouSum / static_cast<double>(positiveRows.size());
        }
        const torch::Tensor totalLoss = boxLoss * config.boxWeight + classLoss * config.classWeight;
        if (!torch::isfinite(totalLoss).all().item<bool>()) return foundation::Result<Yolo11DetectionLossMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "YOLO11 detection loss is NaN or infinite"));
        return foundation::Result<Yolo11DetectionLossMetrics>::Success({totalLoss, boxLoss.item<double>(), classLoss.item<double>(), static_cast<int>(positiveRows.size()), static_cast<int>(assignedGroundTruth.size()), meanIou});
    }
    catch (const c10::Error &error) { return foundation::Result<Yolo11DetectionLossMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch YOLO11 detection loss failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<Yolo11DetectionLossMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("YOLO11 detection loss failed: ") + error.what())); }
}

foundation::Result<Yolo11AugmentedSample> FlipYolo11DetectionSampleHorizontally(const torch::Tensor &image, const std::vector<Yolo11GroundTruthDetection> &targets, const int classCount, const float imageWidth)
{
    if (!image.defined()) return foundation::Result<Yolo11AugmentedSample>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 augmentation image must be defined"));
    if (image.dim() != 3 || image.size(0) <= 0 || image.size(1) <= 0 || image.size(2) <= 0) return foundation::Result<Yolo11AugmentedSample>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 augmentation image must have shape [channels, height, width]"));
    if (image.scalar_type() != torch::kFloat32) return foundation::Result<Yolo11AugmentedSample>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 augmentation image must be float32"));
    if (!std::isfinite(imageWidth) || imageWidth <= 0.0F || std::fabs(imageWidth - static_cast<float>(image.size(2))) > 1.0e-4F) return foundation::Result<Yolo11AugmentedSample>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 augmentation imageWidth must match the tensor width"));
    const auto validation = ValidateGroundTruthTargets(targets, classCount);
    if (!validation.IsSuccess()) return foundation::Result<Yolo11AugmentedSample>::Failure(validation.Failure());
    try
    {
        if (!torch::isfinite(image).all().item<bool>()) return foundation::Result<Yolo11AugmentedSample>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 augmentation image contains a non-finite value"));
        std::vector<Yolo11GroundTruthDetection> flippedTargets;
        flippedTargets.reserve(targets.size());
        for (const Yolo11GroundTruthDetection &target : targets)
        {
            const float newX1 = imageWidth - target.box.x2;
            const float newX2 = imageWidth - target.box.x1;
            flippedTargets.push_back({{newX1, target.box.y1, newX2, target.box.y2}, target.classIndex});
            if (!IsValidBox(flippedTargets.back().box)) return foundation::Result<Yolo11AugmentedSample>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "YOLO11 horizontal flip produced an invalid box"));
        }
        return foundation::Result<Yolo11AugmentedSample>::Success({image.flip({2}).contiguous(), std::move(flippedTargets)});
    }
    catch (const c10::Error &error) { return foundation::Result<Yolo11AugmentedSample>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch YOLO11 augmentation failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<Yolo11AugmentedSample>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("YOLO11 augmentation failed: ") + error.what())); }
}

foundation::Result<Yolo11DetectionEvaluationSummary> EvaluateYolo11DetectionMetrics(const std::vector<models::common::Detection> &predictions, const std::vector<Yolo11GroundTruthDetection> &groundTruth, const int classCount, const Yolo11DetectionMetricsConfig &config)
{
    const auto groundTruthValidation = ValidateGroundTruthTargets(groundTruth, classCount);
    if (!groundTruthValidation.IsSuccess()) return foundation::Result<Yolo11DetectionEvaluationSummary>::Failure(groundTruthValidation.Failure());
    if (!std::isfinite(config.iouThreshold) || config.iouThreshold < 0.0F || config.iouThreshold > 1.0F || !std::isfinite(config.scoreThreshold)) return foundation::Result<Yolo11DetectionEvaluationSummary>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 metric thresholds must be finite, and IoU threshold must be in the closed interval from zero to one"));
    std::vector<models::common::Detection> filtered;
    filtered.reserve(predictions.size());
    for (const models::common::Detection &prediction : predictions)
    {
        if (!IsValidBox(prediction.box)) return foundation::Result<Yolo11DetectionEvaluationSummary>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 prediction boxes must have positive finite area"));
        if (prediction.classIndex < 0 || prediction.classIndex >= classCount) return foundation::Result<Yolo11DetectionEvaluationSummary>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 prediction class index is outside classCount"));
        if (!std::isfinite(prediction.score)) return foundation::Result<Yolo11DetectionEvaluationSummary>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 prediction score must be finite"));
        if (prediction.score >= config.scoreThreshold) filtered.push_back(prediction);
    }
    std::sort(filtered.begin(), filtered.end(), [](const models::common::Detection &left, const models::common::Detection &right) { return left.score > right.score; });
    std::vector<bool> matchedGroundTruth(groundTruth.size(), false);
    Yolo11DetectionEvaluationSummary summary;
    double matchedIouSum = 0.0;
    for (const models::common::Detection &prediction : filtered)
    {
        float bestIou = -1.0F;
        size_t bestIndex = groundTruth.size();
        for (size_t targetIndex = 0; targetIndex < groundTruth.size(); ++targetIndex)
        {
            if (matchedGroundTruth[targetIndex] || groundTruth[targetIndex].classIndex != prediction.classIndex) continue;
            const float iou = models::common::IntersectionOverUnion(prediction.box, groundTruth[targetIndex].box);
            if (iou > bestIou)
            {
                bestIou = iou;
                bestIndex = targetIndex;
            }
        }
        if (bestIndex < groundTruth.size() && bestIou >= config.iouThreshold)
        {
            matchedGroundTruth[bestIndex] = true;
            ++summary.truePositive;
            matchedIouSum += bestIou;
        }
        else
        {
            ++summary.falsePositive;
        }
    }
    for (const bool matched : matchedGroundTruth)
    {
        if (!matched) ++summary.falseNegative;
    }
    const int predictedPositive = summary.truePositive + summary.falsePositive;
    const int actualPositive = summary.truePositive + summary.falseNegative;
    summary.precision = predictedPositive > 0 ? static_cast<double>(summary.truePositive) / static_cast<double>(predictedPositive) : 0.0;
    summary.recall = actualPositive > 0 ? static_cast<double>(summary.truePositive) / static_cast<double>(actualPositive) : 0.0;
    summary.meanMatchedIou = summary.truePositive > 0 ? matchedIouSum / static_cast<double>(summary.truePositive) : 0.0;
    return foundation::Result<Yolo11DetectionEvaluationSummary>::Success(summary);
}
}
