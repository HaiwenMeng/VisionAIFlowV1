#include "visionaiflow/training/LinearClassifierTrainer.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <cmath>
#include <exception>
#include <limits>
#include <vector>

namespace visionaiflow::training
{
namespace
{
foundation::Result<void> ValidateBatch(const torch::Tensor &features, const torch::Tensor &targets)
{
    try
    {
        if (!features.defined() || !targets.defined()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Training features and targets must be defined tensors"));
        if (features.dim() != 2 || targets.dim() != 1) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Classification training requires rank-two features and rank-one targets"));
        if (features.size(0) <= 0 || features.size(1) <= 0 || targets.size(0) != features.size(0)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Training feature and target dimensions do not match"));
        if (features.scalar_type() != torch::kFloat32 || targets.scalar_type() != torch::kInt64) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Training features must be float32 and targets must be int64"));
        if (features.device() != targets.device()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Training features and targets must use the same device"));
        if (!torch::isfinite(features).all().item<bool>()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Training features contain a NaN or infinite value"));
        return foundation::Result<void>::Success();
    }
    catch (const c10::Error &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch classification batch validation failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("Classification batch validation failed: ") + error.what())); }
}

foundation::Result<TrainingMetrics> CreateMetrics(const torch::Tensor &logits, const torch::Tensor &targets, const torch::Tensor &loss)
{
    if (!torch::isfinite(loss).all().item<bool>()) return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Training loss is NaN or infinite"));
    const torch::Tensor predictions = logits.argmax(1);
    const double accuracy = predictions.eq(targets).to(torch::kFloat32).mean().item<double>();
    const double scalarLoss = loss.item<double>();
    if (!std::isfinite(scalarLoss) || !std::isfinite(accuracy)) return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Training metrics are not finite"));
    return foundation::Result<TrainingMetrics>::Success({scalarLoss, accuracy, targets.size(0)});
}

foundation::Result<void> ValidateMultiLabelBatch(const torch::Tensor &features, const torch::Tensor &targets)
{
    try
    {
        if (!features.defined() || !targets.defined()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Multi-label features and targets must be defined tensors"));
        if (features.dim() != 2 || targets.dim() != 2 || targets.size(0) != features.size(0) || targets.size(0) <= 0 || targets.size(1) <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Multi-label training requires matching rank-two feature and target tensors"));
        if (features.scalar_type() != torch::kFloat32 || targets.scalar_type() != torch::kFloat32 || features.device() != targets.device()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Multi-label features and targets must be float32 tensors on the same device"));
        if (!torch::isfinite(features).all().item<bool>() || !torch::isfinite(targets).all().item<bool>()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Multi-label training tensors contain a NaN or infinite value"));
        if (torch::any(targets.lt(0.0F)).item<bool>() || torch::any(targets.gt(1.0F)).item<bool>()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Multi-label targets must be in the closed interval from zero to one"));
        return foundation::Result<void>::Success();
    }
    catch (const c10::Error &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch multi-label batch validation failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("Multi-label batch validation failed: ") + error.what())); }
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

foundation::Result<TrainingMetrics> CreateMultiLabelMetrics(const torch::Tensor &logits, const torch::Tensor &targets, const torch::Tensor &loss)
{
    if (!torch::isfinite(loss).all().item<bool>()) return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Multi-label training loss is NaN or infinite"));
    const double accuracy = logits.sigmoid().ge(0.5F).eq(targets.ge(0.5F)).to(torch::kFloat32).mean().item<double>();
    const double scalarLoss = loss.item<double>();
    if (!std::isfinite(scalarLoss) || !std::isfinite(accuracy)) return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Multi-label training metrics are not finite"));
    return foundation::Result<TrainingMetrics>::Success({scalarLoss, accuracy, targets.size(0)});
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
    return foundation::Result<std::vector<int64_t>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Linear classifier model parameter is missing from the runtime module"));
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

foundation::Result<void> ValidateManifestParameterNames(const QJsonObject &manifest, const QStringList &expectedNames)
{
    const QJsonValue value = manifest.value(QStringLiteral("parameterNames"));
    if (!value.isArray()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterNames must be an array"));
    const QJsonArray actual = value.toArray();
    if (actual.size() != expectedNames.size()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterNames count does not match the model contract"));
    for (qsizetype index = 0; index < expectedNames.size(); ++index)
    {
        if (!actual.at(index).isString() || actual.at(index).toString() != expectedNames.at(index)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterNames do not match the model contract"));
    }
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidateManifestParameterShapes(const QJsonObject &manifest, const QStringList &expectedNames, const torch::nn::Module &model)
{
    const QJsonValue value = manifest.value(QStringLiteral("parameterShapes"));
    if (!value.isArray()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterShapes must be an array"));
    const QJsonArray actual = value.toArray();
    if (actual.size() != expectedNames.size()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterShapes count does not match the model contract"));
    for (qsizetype index = 0; index < expectedNames.size(); ++index)
    {
        if (!actual.at(index).isObject()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterShapes entries must be objects"));
        const QJsonObject entry = actual.at(index).toObject();
        if (entry.value(QStringLiteral("name")).toString() != expectedNames.at(index)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest parameterShapes names do not match the model contract"));
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

foundation::Result<void> WriteCheckpointMetadata(const QString &path, const QByteArray &bytes, const torch::nn::Module &model, const TrainingCheckpointState &state)
{
    const auto hash = Sha256Bytes(bytes);
    if (!hash.IsSuccess()) return foundation::Result<void>::Failure(hash.Failure());
    const QStringList expectedParameterNames = LinearClassifierParameterNames();
    const auto parameterShapes = ParameterShapesToJson(model, expectedParameterNames);
    if (!parameterShapes.IsSuccess()) return foundation::Result<void>::Failure(parameterShapes.Failure());
    const auto trainingState = TrainingCheckpointStateToJson(state);
    if (!trainingState.IsSuccess()) return foundation::Result<void>::Failure(trainingState.Failure());
    const QFileInfo fileInfo(path);
    const QJsonObject manifest{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("schemaName"), QStringLiteral("linear_classifier")},
        {QStringLiteral("archiveFile"), fileInfo.fileName()},
        {QStringLiteral("archiveBytes"), bytes.size()},
        {QStringLiteral("archiveSha256"), QString::fromLatin1(hash.Value())},
        {QStringLiteral("productId"), QStringLiteral("VisionAIFlowV1")},
        {QStringLiteral("adapterId"), QStringLiteral("visionaiflow.linear_classifier")},
        {QStringLiteral("adapterVersion"), QStringLiteral("0.1.0")},
        {QStringLiteral("libtorchVersion"), QString::fromLatin1(TORCH_VERSION)},
        {QStringLiteral("parameterNames"), ParameterNamesToJson(expectedParameterNames)},
        {QStringLiteral("parameterShapes"), parameterShapes.Value()},
        {QStringLiteral("trainingState"), trainingState.Value()}};
    const auto manifestWritten = WriteTextAtomically(CheckpointManifestPath(path), QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    if (!manifestWritten.IsSuccess()) return manifestWritten;
    const QByteArray hashBytes = hash.Value() + QByteArrayLiteral("  ") + fileInfo.fileName().toUtf8() + QByteArrayLiteral("\n");
    return WriteTextAtomically(CheckpointHashPath(path), hashBytes);
}

foundation::Result<TrainingCheckpointState> VerifyCheckpointMetadata(const QString &path, const QByteArray &bytes, const torch::nn::Module &model)
{
    const auto manifestResult = ReadJsonObject(CheckpointManifestPath(path));
    if (!manifestResult.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(manifestResult.Failure());
    const QJsonObject manifest = manifestResult.Value();
    if (manifest.value(QStringLiteral("schemaVersion")).toInt() != 1) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest schema version is unsupported"));
    if (manifest.value(QStringLiteral("schemaName")).toString() != QStringLiteral("linear_classifier")) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest schema name does not match linear classifier"));
    if (manifest.value(QStringLiteral("archiveFile")).toString() != QFileInfo(path).fileName()) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest archive file does not match checkpoint path"));
    if (static_cast<qint64>(manifest.value(QStringLiteral("archiveBytes")).toDouble(-1.0)) != bytes.size()) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest byte count does not match checkpoint file"));
    if (manifest.value(QStringLiteral("productId")).toString() != QStringLiteral("VisionAIFlowV1")) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest product id is unsupported"));
    if (manifest.value(QStringLiteral("adapterId")).toString() != QStringLiteral("visionaiflow.linear_classifier")) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest adapter id is unsupported"));
    if (manifest.value(QStringLiteral("libtorchVersion")).toString() != QString::fromLatin1(TORCH_VERSION)) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint manifest LibTorch version does not match this build"));
    const QStringList expectedParameterNames = LinearClassifierParameterNames();
    const auto parameterNames = ValidateManifestParameterNames(manifest, expectedParameterNames);
    if (!parameterNames.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(parameterNames.Failure());
    const auto parameterShapes = ValidateManifestParameterShapes(manifest, expectedParameterNames, model);
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
}

LinearClassifierImpl::LinearClassifierImpl(const int64_t inputFeatures, const int64_t classCount)
{
    m_linear = register_module("linear", torch::nn::Linear(torch::nn::LinearOptions(inputFeatures, classCount)));
}

torch::Tensor LinearClassifierImpl::forward(const torch::Tensor &features) { return m_linear->forward(features); }
torch::Tensor LinearClassifierImpl::Weight() const { return m_linear->weight; }
torch::Tensor LinearClassifierImpl::Bias() const { return m_linear->bias; }

foundation::Result<LinearClassifier> CreateLinearClassifier(const int64_t inputFeatures, const int64_t classCount)
{
    if (inputFeatures <= 0 || classCount < 2) return foundation::Result<LinearClassifier>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Linear classifier requires positive input features and at least two classes"));
    try { return foundation::Result<LinearClassifier>::Success(LinearClassifier(inputFeatures, classCount)); }
    catch (const c10::Error &error) { return foundation::Result<LinearClassifier>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch classifier construction failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<LinearClassifier>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("Classifier construction failed: ") + error.what())); }
}

QStringList LinearClassifierParameterNames()
{
    return {QStringLiteral("linear.weight"), QStringLiteral("linear.bias")};
}

foundation::Result<TrainingMetrics> TrainClassificationStep(LinearClassifier &model, torch::optim::Optimizer &optimizer, const torch::Tensor &features, const torch::Tensor &targets)
{
    if (!model) return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Linear classifier model must not be null"));
    const auto validation = ValidateBatch(features, targets);
    if (!validation.IsSuccess()) return foundation::Result<TrainingMetrics>::Failure(validation.Failure());
    try
    {
        model->train(); optimizer.zero_grad();
        const torch::Tensor logits = model->forward(features);
        const torch::Tensor loss = torch::nn::functional::cross_entropy(logits, targets);
        const auto metrics = CreateMetrics(logits, targets, loss);
        if (!metrics.IsSuccess()) return metrics;
        loss.backward();
        const auto gradients = ValidateFiniteGradients(*model, "Training step");
        if (!gradients.IsSuccess()) { optimizer.zero_grad(); return foundation::Result<TrainingMetrics>::Failure(gradients.Failure()); }
        optimizer.step();
        return metrics;
    }
    catch (const c10::Error &error) { return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch training step failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("Training step failed: ") + error.what())); }
}

foundation::Result<TrainingMetrics> EvaluateClassificationBatch(LinearClassifier &model, const torch::Tensor &features, const torch::Tensor &targets)
{
    if (!model) return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Linear classifier model must not be null"));
    const auto validation = ValidateBatch(features, targets);
    if (!validation.IsSuccess()) return foundation::Result<TrainingMetrics>::Failure(validation.Failure());
    try
    {
        torch::NoGradGuard guard; model->eval();
        const torch::Tensor logits = model->forward(features);
        return CreateMetrics(logits, targets, torch::nn::functional::cross_entropy(logits, targets));
    }
    catch (const c10::Error &error) { return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch evaluation failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("Evaluation failed: ") + error.what())); }
}

foundation::Result<TrainingMetrics> TrainMultiLabelClassificationStep(LinearClassifier &model, torch::optim::Optimizer &optimizer, const torch::Tensor &features, const torch::Tensor &targets)
{
    if (!model) return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Linear classifier model must not be null"));
    const auto validation = ValidateMultiLabelBatch(features, targets);
    if (!validation.IsSuccess()) return foundation::Result<TrainingMetrics>::Failure(validation.Failure());
    try
    {
        model->train(); optimizer.zero_grad();
        const torch::Tensor logits = model->forward(features);
        if (logits.sizes() != targets.sizes()) return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Multi-label target class count does not match model output"));
        const torch::Tensor loss = torch::nn::functional::binary_cross_entropy_with_logits(logits, targets);
        const auto metrics = CreateMultiLabelMetrics(logits, targets, loss);
        if (!metrics.IsSuccess()) return metrics;
        loss.backward();
        const auto gradients = ValidateFiniteGradients(*model, "Multi-label training step");
        if (!gradients.IsSuccess()) { optimizer.zero_grad(); return foundation::Result<TrainingMetrics>::Failure(gradients.Failure()); }
        optimizer.step();
        return metrics;
    }
    catch (const c10::Error &error) { return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch multi-label training step failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("Multi-label training step failed: ") + error.what())); }
}

foundation::Result<TrainingMetrics> EvaluateMultiLabelClassificationBatch(LinearClassifier &model, const torch::Tensor &features, const torch::Tensor &targets)
{
    if (!model) return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Linear classifier model must not be null"));
    const auto validation = ValidateMultiLabelBatch(features, targets);
    if (!validation.IsSuccess()) return foundation::Result<TrainingMetrics>::Failure(validation.Failure());
    try
    {
        torch::NoGradGuard guard; model->eval();
        const torch::Tensor logits = model->forward(features);
        if (logits.sizes() != targets.sizes()) return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Multi-label target class count does not match model output"));
        return CreateMultiLabelMetrics(logits, targets, torch::nn::functional::binary_cross_entropy_with_logits(logits, targets));
    }
    catch (const c10::Error &error) { return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch multi-label evaluation failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<TrainingMetrics>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("Multi-label evaluation failed: ") + error.what())); }
}

foundation::Result<void> SaveTrainingCheckpoint(const QString &path, LinearClassifier &model, torch::optim::Optimizer &optimizer)
{
    return SaveTrainingCheckpoint(path, model, optimizer, TrainingCheckpointState{});
}

foundation::Result<void> SaveTrainingCheckpoint(const QString &path, LinearClassifier &model, torch::optim::Optimizer &optimizer, const TrainingCheckpointState &state)
{
    if (path.isEmpty() || !model) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint path and model must be valid"));
    const QFileInfo fileInfo(path);
    if (!fileInfo.dir().exists()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint parent directory does not exist"));
    const auto stateValidation = ValidateTrainingCheckpointState(state);
    if (!stateValidation.IsSuccess()) return stateValidation;
    try
    {
        torch::serialize::OutputArchive root;
        torch::serialize::OutputArchive modelArchive;
        torch::serialize::OutputArchive optimizerArchive;
        model->save(modelArchive);
        optimizer.save(optimizerArchive);
        root.write("schemaVersion", static_cast<int64_t>(1));
        root.write("schemaName", std::string("linear_classifier"));
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
        return WriteCheckpointMetadata(path, bytes, *model, state);
    }
    catch (const c10::Error &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch checkpoint save failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, std::string("Checkpoint save failed: ") + error.what())); }
}

foundation::Result<void> LoadTrainingCheckpoint(const QString &path, LinearClassifier &model, torch::optim::Optimizer &optimizer, const torch::Device &device)
{
    TrainingCheckpointState ignoredState;
    return LoadTrainingCheckpoint(path, model, optimizer, device, ignoredState);
}

foundation::Result<void> LoadTrainingCheckpoint(const QString &path, LinearClassifier &model, torch::optim::Optimizer &optimizer, const torch::Device &device, TrainingCheckpointState &state)
{
    return LoadTrainingCheckpoint(path, model, optimizer, device, state, TrainingCheckpointLoadOptions{});
}

foundation::Result<void> LoadTrainingCheckpoint(const QString &path, LinearClassifier &model, torch::optim::Optimizer &optimizer, const torch::Device &device, TrainingCheckpointState &state, const TrainingCheckpointLoadOptions &options)
{
    if (path.isEmpty() || !model) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint path and model must be valid"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to open checkpoint for reading: ").append(file.errorString()).toStdString()));
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint file is empty"));
    const auto metadataVerified = VerifyCheckpointMetadata(path, bytes, *model);
    if (!metadataVerified.IsSuccess()) return foundation::Result<void>::Failure(metadataVerified.Failure());
    try
    {
        torch::serialize::InputArchive root;
        root.load_from(bytes.constData(), static_cast<size_t>(bytes.size()), device);
        c10::IValue schemaVersion;
        c10::IValue schemaName;
        root.read("schemaVersion", schemaVersion);
        root.read("schemaName", schemaName);
        if (!schemaVersion.isInt() || schemaVersion.toInt() != 1) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint schema version is unsupported"));
        if (!schemaName.isString() || schemaName.toStringRef() != "linear_classifier") return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint schema name does not match linear classifier"));
        torch::serialize::InputArchive modelArchive;
        torch::serialize::InputArchive optimizerArchive;
        root.read("model", modelArchive);
        root.read("optimizer", optimizerArchive);
        const auto archiveState = ReadTrainingCheckpointStateArchive(root, options.restoreCpuRng);
        if (!archiveState.IsSuccess()) return foundation::Result<void>::Failure(archiveState.Failure());
        const auto stateMatchesManifest = ValidateTrainingCheckpointStateMatch(metadataVerified.Value(), archiveState.Value());
        if (!stateMatchesManifest.IsSuccess()) return stateMatchesManifest;
        model->load(modelArchive);
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
