#pragma once

#include "visionaiflow/models/classification/linear/LinearClassifier.h"

namespace visionaiflow::training
{
using LinearClassifierImpl = models::classification::linear::LinearClassifierImpl;
using LinearClassifier = models::classification::linear::LinearClassifier;
using TrainingMetrics = models::classification::linear::TrainingMetrics;
using models::classification::linear::CreateLinearClassifier;
using models::classification::linear::LinearClassifierParameterNames;
using models::classification::linear::TrainClassificationStep;
using models::classification::linear::EvaluateClassificationBatch;
using models::classification::linear::TrainMultiLabelClassificationStep;
using models::classification::linear::EvaluateMultiLabelClassificationBatch;
using models::classification::linear::SaveTrainingCheckpoint;
using models::classification::linear::LoadTrainingCheckpoint;
}
