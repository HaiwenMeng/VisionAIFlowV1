#include "visionaiflow/training/TensorDataLoader.h"

#include <algorithm>
#include <exception>
#include <numeric>
#include <random>

namespace visionaiflow::training
{
namespace
{
foundation::Result<void> ValidateDataset(const torch::Tensor &features, const torch::Tensor &targets, const DataLoaderOptions &options)
{
    if (!features.defined() || !targets.defined()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Data loader tensors must be defined"));
    if (features.dim() < 2 || targets.dim() != 1 || features.size(0) <= 0 || targets.size(0) != features.size(0)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Data loader feature and target sample dimensions do not match"));
    if (options.batchSize <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Data loader batch size must be positive"));
    if (options.pinMemory && !features.device().is_cpu()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Pinned-memory data loading requires CPU source tensors"));
    return foundation::Result<void>::Success();
}
}

TensorDataLoader::TensorDataLoader(torch::Tensor features, torch::Tensor targets, const DataLoaderOptions options) : m_features(std::move(features)), m_targets(std::move(targets)), m_options(options) {}

foundation::Result<TensorDataLoader> TensorDataLoader::Create(torch::Tensor features, torch::Tensor targets, const DataLoaderOptions options)
{
    const auto validation = ValidateDataset(features, targets, options);
    if (!validation.IsSuccess()) return foundation::Result<TensorDataLoader>::Failure(validation.Failure());
    return foundation::Result<TensorDataLoader>::Success(TensorDataLoader(std::move(features), std::move(targets), options));
}

foundation::Result<std::vector<TensorBatch>> TensorDataLoader::NextEpoch()
{
    try
    {
        const int64_t sampleCount = m_features.size(0);
        std::vector<int64_t> order(static_cast<size_t>(sampleCount));
        std::iota(order.begin(), order.end(), 0);
        if (m_options.shuffle)
        {
            std::mt19937_64 generator(m_options.randomSeed + m_epoch);
            std::shuffle(order.begin(), order.end(), generator);
        }
        ++m_epoch;
        std::vector<TensorBatch> batches;
        for (int64_t offset = 0; offset < sampleCount; offset += m_options.batchSize)
        {
            const int64_t count = std::min(m_options.batchSize, sampleCount - offset);
            std::vector<int64_t> indices(order.begin() + offset, order.begin() + offset + count);
            const torch::Tensor indexTensor = torch::tensor(indices, torch::TensorOptions().dtype(torch::kInt64).device(m_features.device()));
            torch::Tensor batchFeatures = m_features.index_select(0, indexTensor);
            torch::Tensor batchTargets = m_targets.index_select(0, indexTensor);
            if (m_options.pinMemory)
            {
                batchFeatures = batchFeatures.pin_memory();
                batchTargets = batchTargets.pin_memory();
            }
            batches.push_back({std::move(batchFeatures), std::move(batchTargets)});
        }
        return foundation::Result<std::vector<TensorBatch>>::Success(std::move(batches));
    }
    catch (const c10::Error &error) { return foundation::Result<std::vector<TensorBatch>>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch data loading failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<std::vector<TensorBatch>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("Data loading failed: ") + error.what())); }
}

int64_t TensorDataLoader::SampleCount() const noexcept { return m_features.size(0); }
}
