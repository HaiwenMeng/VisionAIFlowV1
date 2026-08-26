#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>
#include <QStringList>

#include <cstdint>

#if defined(VISIONAIFLOW_EXPORT_LIBRARY)
#define VISIONAIFLOW_EXPORT_API __declspec(dllexport)
#else
#define VISIONAIFLOW_EXPORT_API __declspec(dllimport)
#endif

namespace visionaiflow::exporter
{
struct ClassificationPackageMetadata final
{
    QString modelId;
    QString packageId;
    QString packageVersion;
    QString adapterId;
    QString adapterVersion;
    QString trainingRunId;
    QString datasetId;
    QString trainingConfigSha256;
    QString sourceCheckpointSha256;
    QString exporterProductVersion;
    QString minSupportedProductVersion;
    QString maxSupportedProductVersion;
    QString licenseId;
    QString licenseName;
    QStringList labels;
    int64_t inputFeatures{0};
    int64_t classCount{0};
    int artifactContractVersion{0};
};

struct Yolo11DetectionPackageMetadata final
{
    QString modelId;
    QString packageId;
    QString packageVersion;
    QString adapterId;
    QString adapterVersion;
    QString trainingRunId;
    QString datasetId;
    QString trainingConfigSha256;
    QString sourceCheckpointSha256;
    QString exporterProductVersion;
    QString minSupportedProductVersion;
    QString maxSupportedProductVersion;
    QString licenseId;
    QString licenseName;
    QStringList labels;
    int64_t inputChannels{0};
    int64_t imageHeight{0};
    int64_t imageWidth{0};
    int64_t rowCount{0};
    int64_t classCount{0};
    int artifactContractVersion{0};
};

struct ModelPackageRuntimeContract final
{
    QString packageRoot;
    QString modelId;
    QString adapterVersion;
    QString projectType;
    QString decoderId;
    QString artifactPath;
    int artifactContractVersion{0};
};

VISIONAIFLOW_EXPORT_API foundation::Result<void> CreateUnsignedClassificationModelPackage(const QString &packageRoot, const QString &onnxPath, const ClassificationPackageMetadata &metadata);
VISIONAIFLOW_EXPORT_API foundation::Result<void> CreateUnsignedYolo11DetectionModelPackage(const QString &packageRoot, const QString &onnxPath, const Yolo11DetectionPackageMetadata &metadata);
VISIONAIFLOW_EXPORT_API foundation::Result<void> VerifyModelPackage(const QString &packageRoot, bool requireSignature);
VISIONAIFLOW_EXPORT_API foundation::Result<ModelPackageRuntimeContract> LoadVerifiedModelPackageRuntimeContract(const QString &packageRoot, const QString &runtimeId, bool requireSignature);
VISIONAIFLOW_EXPORT_API foundation::Result<void> InstallModelPackage(const QString &sourcePackageRoot, const QString &destinationPackageRoot, bool requireSignature);
}
