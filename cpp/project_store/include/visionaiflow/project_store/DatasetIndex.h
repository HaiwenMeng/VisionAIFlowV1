#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QSize>
#include <QString>

#include <vector>

namespace visionaiflow::project_store
{
struct DatasetImage final
{
    QString imageId;
    QString fileName;
    QString relativePath;
    QString sha256;
    qint64 bytes{0};
    QSize size;
    QString importedUtc;
};

class DatasetIndex final
{
public:
    foundation::Result<std::vector<DatasetImage>> Load(const QString &projectRoot) const;
    foundation::Result<DatasetImage> ImportImage(const QString &projectRoot, const QString &sourcePath) const;
};
}
