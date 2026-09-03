#pragma once

#include <QString>

#include <memory>

namespace torch::jit
{
struct Graph;
}

namespace visionaiflow::yolov11
{
bool ConvertYolo11TraceToOnnx(const std::shared_ptr<torch::jit::Graph> &graph, QString *errorMessage);
}
