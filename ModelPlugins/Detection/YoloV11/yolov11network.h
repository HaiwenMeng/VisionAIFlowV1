#pragma once

#ifdef slots
#undef slots
#endif

#include <torch/torch.h>

#include <vector>

namespace visionaiflow::yolov11
{
class ConvImpl final : public torch::nn::Module
{
public:
    ConvImpl(int64_t inputChannels, int64_t outputChannels, int64_t kernelSize = 1, int64_t stride = 1, int64_t groups = 1,
             bool activation = true);

    torch::Tensor forward(const torch::Tensor &input);

private:
    torch::nn::Conv2d m_conv{nullptr};
    torch::nn::BatchNorm2d m_bn{nullptr};
    torch::nn::SiLU m_act{nullptr};
    bool m_activation{true};
};
TORCH_MODULE(Conv);

class BottleneckImpl final : public torch::nn::Module
{
public:
    BottleneckImpl(int64_t inputChannels, int64_t outputChannels, bool shortcut = true, int64_t groups = 1,
                   std::pair<int64_t, int64_t> kernels = {3, 3}, double expansion = 0.5);

    torch::Tensor forward(const torch::Tensor &input);

private:
    Conv m_cv1{nullptr};
    Conv m_cv2{nullptr};
    bool m_add{false};
};
TORCH_MODULE(Bottleneck);

class C3kImpl final : public torch::nn::Module
{
public:
    C3kImpl(int64_t inputChannels, int64_t outputChannels, int64_t repeats = 1, bool shortcut = true, int64_t groups = 1,
            int64_t kernelSize = 3, double expansion = 0.5);

    torch::Tensor forward(const torch::Tensor &input);

private:
    Conv m_cv1{nullptr};
    Conv m_cv2{nullptr};
    Conv m_cv3{nullptr};
    torch::nn::ModuleList m_m{nullptr};
};
TORCH_MODULE(C3k);

class C3k2Impl final : public torch::nn::Module
{
public:
    C3k2Impl(int64_t inputChannels, int64_t outputChannels, int64_t repeats, bool c3k, bool shortcut = false, int64_t groups = 1,
             double expansion = 0.5);

    torch::Tensor forward(const torch::Tensor &input);

private:
    Conv m_cv1{nullptr};
    Conv m_cv2{nullptr};
    torch::nn::ModuleList m_m{nullptr};
    int64_t m_hiddenChannels{0};
    bool m_useC3k{false};
};
TORCH_MODULE(C3k2);

class SPPFImpl final : public torch::nn::Module
{
public:
    SPPFImpl(int64_t inputChannels, int64_t outputChannels, int64_t kernelSize = 5);

    torch::Tensor forward(const torch::Tensor &input);

private:
    Conv m_cv1{nullptr};
    Conv m_cv2{nullptr};
    torch::nn::MaxPool2d m_maxPool{nullptr};
};
TORCH_MODULE(SPPF);

class AttentionImpl final : public torch::nn::Module
{
public:
    AttentionImpl(int64_t dimensions, int64_t heads, double attentionRatio = 0.5);

    torch::Tensor forward(const torch::Tensor &input);

private:
    Conv m_qkv{nullptr};
    Conv m_proj{nullptr};
    Conv m_pe{nullptr};
    int64_t m_heads{0};
    int64_t m_keyDimensions{0};
    double m_scale{1.0};
};
TORCH_MODULE(Attention);

class PSABlockImpl final : public torch::nn::Module
{
public:
    PSABlockImpl(int64_t channels, double attentionRatio = 0.5, int64_t heads = 4, bool add = true);

    torch::Tensor forward(const torch::Tensor &input);

private:
    Attention m_attn{nullptr};
    torch::nn::Sequential m_ffn{nullptr};
    bool m_add{true};
};
TORCH_MODULE(PSABlock);

class C2PSAImpl final : public torch::nn::Module
{
public:
    C2PSAImpl(int64_t inputChannels, int64_t outputChannels, int64_t repeats = 1, double expansion = 0.5);

    torch::Tensor forward(const torch::Tensor &input);

private:
    Conv m_cv1{nullptr};
    Conv m_cv2{nullptr};
    torch::nn::Sequential m_m{nullptr};
    int64_t m_hiddenChannels{0};
};
TORCH_MODULE(C2PSA);

class DFLImpl final : public torch::nn::Module
{
public:
    explicit DFLImpl(int64_t channels = 16);

    torch::Tensor forward(const torch::Tensor &input);

private:
    torch::nn::Conv2d m_conv{nullptr};
    int64_t m_channels{16};
};
TORCH_MODULE(DFL);

class DetectBoxBranchImpl final : public torch::nn::Module
{
public:
    DetectBoxBranchImpl(int64_t inputChannels, int64_t boxChannels, int64_t outputChannels);

    torch::Tensor forward(const torch::Tensor &input);

private:
    Conv m_first{nullptr};
    Conv m_second{nullptr};
    torch::nn::Conv2d m_output{nullptr};
};
TORCH_MODULE(DetectBoxBranch);

class DetectClassBranchImpl final : public torch::nn::Module
{
public:
    DetectClassBranchImpl(int64_t inputChannels, int64_t classChannels, int64_t classCount);

    torch::Tensor forward(const torch::Tensor &input);

private:
    torch::nn::Sequential m_first{nullptr};
    torch::nn::Sequential m_second{nullptr};
    torch::nn::Conv2d m_output{nullptr};
};
TORCH_MODULE(DetectClassBranch);

class DetectImpl final : public torch::nn::Module
{
public:
    DetectImpl(int64_t classCount, const std::vector<int64_t> &inputChannels);

    std::vector<torch::Tensor> forward(const std::vector<torch::Tensor> &input);

private:
    int64_t m_classCount{0};
    int64_t m_regMax{16};
    torch::nn::ModuleList m_cv2{nullptr};
    torch::nn::ModuleList m_cv3{nullptr};
    DFL m_dfl{nullptr};
};
TORCH_MODULE(Detect);

class Yolo11NetworkImpl final : public torch::nn::Module
{
public:
    explicit Yolo11NetworkImpl(int64_t classCount);

    std::vector<torch::Tensor> forward(const torch::Tensor &input);
    int64_t classCount() const;

private:
    torch::nn::ModuleList m_model{nullptr};
    int64_t m_classCount{0};
};
TORCH_MODULE(Yolo11Network);
} // namespace visionaiflow::yolov11
