#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>
#include <QStringList>

#include <cstdint>

namespace visionaiflow::exporter
{
struct ClassificationPackageMetadata final
{
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
};

struct Yolo11DetectionPackageMetadata final
{
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
};

foundation::Result<void> CreateUnsignedClassificationModelPackage(const QString &packageRoot, const QString &onnxPath, const ClassificationPackageMetadata &metadata);
foundation::Result<void> CreateUnsignedYolo11DetectionModelPackage(const QString &packageRoot, const QString &onnxPath, const Yolo11DetectionPackageMetadata &metadata);
foundation::Result<void> VerifyModelPackage(const QString &packageRoot, bool requireSignature);
foundation::Result<void> InstallModelPackage(const QString &sourcePackageRoot, const QString &destinationPackageRoot, bool requireSignature);
}
