#pragma once

#include <QString>
#include <QVector>

namespace visionaiflow::models::api
{
struct TensorSignature
{
    QString name;
    QString dataType;
    QVector<qint64> shape;
};

struct ModelSignature
{
    QVector<TensorSignature> inputs;
    QVector<TensorSignature> outputs;
    QString decoderId;
};
}
