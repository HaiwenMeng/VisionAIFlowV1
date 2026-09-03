#include "yolov11network.h"

#include <array>
#include <cmath>
#include <numeric>

namespace visionaiflow::yolov11
{
namespace
{
int64_t AutoPadding(const int64_t kernelSize)
{
    return kernelSize / 2;
}
} // namespace

ConvImpl::ConvImpl(const int64_t inputChannels,
                   const int64_t outputChannels,
                   const int64_t kernelSize,
                   const int64_t stride,
                   const int64_t groups,
                   const bool activation)
    : m_activation(activation)
{
    m_conv = register_module("conv",
                             torch::nn::Conv2d(torch::nn::Conv2dOptions(inputChannels, outputChannels, kernelSize)
                                                   .stride(stride)
                                                   .padding(AutoPadding(kernelSize))
                                                   .groups(groups)
                                                   .bias(false)));
    m_bn =
        register_module("bn",
                        torch::nn::BatchNorm2d(torch::nn::BatchNormOptions(outputChannels).eps(0.001).momentum(0.03)));
    m_act = register_module("act", torch::nn::SiLU());
}

torch::Tensor ConvImpl::forward(const torch::Tensor &input)
{
    const torch::Tensor output = m_bn->forward(m_conv->forward(input));
    return m_activation ? m_act->forward(output) : output;
}

BottleneckImpl::BottleneckImpl(const int64_t inputChannels,
                               const int64_t outputChannels,
                               const bool shortcut,
                               const int64_t groups,
                               const std::pair<int64_t, int64_t> kernels,
                               const double expansion)
    : m_add(shortcut && inputChannels == outputChannels)
{
    const int64_t hiddenChannels = static_cast<int64_t>(outputChannels * expansion);
    m_cv1 = register_module("cv1", Conv(inputChannels, hiddenChannels, kernels.first, 1));
    m_cv2 = register_module("cv2", Conv(hiddenChannels, outputChannels, kernels.second, 1, groups));
}

torch::Tensor BottleneckImpl::forward(const torch::Tensor &input)
{
    const torch::Tensor output = m_cv2->forward(m_cv1->forward(input));
    return m_add ? input + output : output;
}

C3kImpl::C3kImpl(const int64_t inputChannels,
                 const int64_t outputChannels,
                 const int64_t repeats,
                 const bool shortcut,
                 const int64_t groups,
                 const int64_t kernelSize,
                 const double expansion)
{
    const int64_t hiddenChannels = static_cast<int64_t>(outputChannels * expansion);
    m_cv1 = register_module("cv1", Conv(inputChannels, hiddenChannels, 1, 1));
    m_cv2 = register_module("cv2", Conv(inputChannels, hiddenChannels, 1, 1));
    m_cv3 = register_module("cv3", Conv(hiddenChannels * 2, outputChannels, 1, 1));
    m_m = register_module("m", torch::nn::ModuleList());
    for (int64_t index = 0; index < repeats; ++index)
    {
        m_m->push_back(
            Bottleneck(hiddenChannels, hiddenChannels, shortcut, groups, std::make_pair(kernelSize, kernelSize), 1.0));
    }
}

torch::Tensor C3kImpl::forward(const torch::Tensor &input)
{
    torch::Tensor first = m_cv1->forward(input);
    for (const std::shared_ptr<torch::nn::Module> &module : *m_m)
    {
        first = module->as<BottleneckImpl>()->forward(first);
    }
    return m_cv3->forward(torch::cat({first, m_cv2->forward(input)}, 1));
}

C3k2Impl::C3k2Impl(const int64_t inputChannels,
                   const int64_t outputChannels,
                   const int64_t repeats,
                   const bool c3k,
                   const bool shortcut,
                   const int64_t groups,
                   const double expansion)
    : m_hiddenChannels(static_cast<int64_t>(outputChannels * expansion)), m_useC3k(c3k)
{
    m_cv1 = register_module("cv1", Conv(inputChannels, 2 * m_hiddenChannels, 1, 1));
    m_cv2 = register_module("cv2", Conv((2 + repeats) * m_hiddenChannels, outputChannels, 1, 1));
    m_m = register_module("m", torch::nn::ModuleList());
    for (int64_t index = 0; index < repeats; ++index)
    {
        if (m_useC3k)
        {
            m_m->push_back(C3k(m_hiddenChannels, m_hiddenChannels, 2, shortcut, groups));
        }
        else
        {
            m_m->push_back(Bottleneck(m_hiddenChannels, m_hiddenChannels, shortcut, groups));
        }
    }
}

torch::Tensor C3k2Impl::forward(const torch::Tensor &input)
{
    std::vector<torch::Tensor> outputs = m_cv1->forward(input).chunk(2, 1);
    for (const std::shared_ptr<torch::nn::Module> &module : *m_m)
    {
        if (m_useC3k)
        {
            outputs.push_back(module->as<C3kImpl>()->forward(outputs.back()));
        }
        else
        {
            outputs.push_back(module->as<BottleneckImpl>()->forward(outputs.back()));
        }
    }
    return m_cv2->forward(torch::cat(outputs, 1));
}

SPPFImpl::SPPFImpl(const int64_t inputChannels, const int64_t outputChannels, const int64_t kernelSize)
{
    const int64_t hiddenChannels = inputChannels / 2;
    m_cv1 = register_module("cv1", Conv(inputChannels, hiddenChannels, 1, 1));
    m_cv2 = register_module("cv2", Conv(hiddenChannels * 4, outputChannels, 1, 1));
    m_maxPool = register_module(
        "m",
        torch::nn::MaxPool2d(torch::nn::MaxPool2dOptions(kernelSize).stride(1).padding(kernelSize / 2)));
}

torch::Tensor SPPFImpl::forward(const torch::Tensor &input)
{
    const torch::Tensor first = m_cv1->forward(input);
    const torch::Tensor second = m_maxPool->forward(first);
    const torch::Tensor third = m_maxPool->forward(second);
    const torch::Tensor fourth = m_maxPool->forward(third);
    return m_cv2->forward(torch::cat({first, second, third, fourth}, 1));
}

AttentionImpl::AttentionImpl(const int64_t dimensions, const int64_t heads, const double attentionRatio)
    : m_heads(heads), m_keyDimensions(static_cast<int64_t>(dimensions / heads * attentionRatio)),
      m_scale(1.0 / std::sqrt(static_cast<double>(m_keyDimensions)))
{
    const int64_t totalKeyDimensions = m_keyDimensions * heads;
    m_qkv = register_module("qkv", Conv(dimensions, dimensions + 2 * totalKeyDimensions, 1, 1, 1, false));
    m_proj = register_module("proj", Conv(dimensions, dimensions, 1, 1, 1, false));
    m_pe = register_module("pe", Conv(dimensions, dimensions, 3, 1, dimensions, false));
}

torch::Tensor AttentionImpl::forward(const torch::Tensor &input)
{
    const int64_t batch = input.size(0);
    const int64_t channels = input.size(1);
    const int64_t height = input.size(2);
    const int64_t width = input.size(3);
    const int64_t area = height * width;
    const torch::Tensor qkv =
        m_qkv->forward(input).view({batch, m_heads, m_keyDimensions * 2 + channels / m_heads, area});
    const std::vector<torch::Tensor> split =
        qkv.split_with_sizes({m_keyDimensions, m_keyDimensions, channels / m_heads}, 2);
    const torch::Tensor query = split[0];
    const torch::Tensor key = split[1];
    const torch::Tensor value = split[2];
    const torch::Tensor attention = torch::matmul(query.transpose(-2, -1), key) * m_scale;
    const torch::Tensor weighted = torch::matmul(value, attention.softmax(-1).transpose(-2, -1));
    const torch::Tensor output = weighted.view({batch, channels, height, width});
    const torch::Tensor positionalEncoding = m_pe->forward(value.reshape({batch, channels, height, width}));
    return m_proj->forward(output + positionalEncoding);
}

PSABlockImpl::PSABlockImpl(const int64_t channels, const double attentionRatio, const int64_t heads, const bool add)
    : m_add(add)
{
    m_attn = register_module("attn", Attention(channels, heads, attentionRatio));
    m_ffn = register_module(
        "ffn",
        torch::nn::Sequential(Conv(channels, channels * 2, 1, 1), Conv(channels * 2, channels, 1, 1, 1, false)));
}

torch::Tensor PSABlockImpl::forward(const torch::Tensor &input)
{
    torch::Tensor output = m_add ? input + m_attn->forward(input) : m_attn->forward(input);
    return m_add ? output + m_ffn->forward(output) : m_ffn->forward(output);
}

C2PSAImpl::C2PSAImpl(const int64_t inputChannels,
                     const int64_t outputChannels,
                     const int64_t repeats,
                     const double expansion)
    : m_hiddenChannels(static_cast<int64_t>(inputChannels * expansion))
{
    m_cv1 = register_module("cv1", Conv(inputChannels, 2 * m_hiddenChannels, 1, 1));
    m_cv2 = register_module("cv2", Conv(2 * m_hiddenChannels, outputChannels, 1, 1));
    m_m = register_module("m", torch::nn::Sequential());
    for (int64_t index = 0; index < repeats; ++index)
    {
        m_m->push_back(PSABlock(m_hiddenChannels, 0.5, m_hiddenChannels / 64));
    }
}

torch::Tensor C2PSAImpl::forward(const torch::Tensor &input)
{
    std::vector<torch::Tensor> outputs = m_cv1->forward(input).chunk(2, 1);
    outputs[1] = m_m->forward(outputs[1]);
    return m_cv2->forward(torch::cat(outputs, 1));
}

DFLImpl::DFLImpl(const int64_t channels) : m_channels(channels)
{
    m_conv = register_module("conv", torch::nn::Conv2d(torch::nn::Conv2dOptions(channels, 1, 1).bias(false)));
    m_conv->weight.set_data(
        torch::arange(channels, torch::TensorOptions().dtype(torch::kFloat32)).view({1, channels, 1, 1}));
    m_conv->weight.set_requires_grad(false);
}

torch::Tensor DFLImpl::forward(const torch::Tensor &input)
{
    const int64_t batch = input.size(0);
    const int64_t anchors = input.size(2);
    return m_conv->forward(input.view({batch, 4, m_channels, anchors}).transpose(1, 2).softmax(1))
        .view({batch, 4, anchors});
}

DetectBoxBranchImpl::DetectBoxBranchImpl(const int64_t inputChannels,
                                         const int64_t boxChannels,
                                         const int64_t outputChannels)
{
    m_first = register_module("0", Conv(inputChannels, boxChannels, 3, 1));
    m_second = register_module("1", Conv(boxChannels, boxChannels, 3, 1));
    m_output = register_module("2", torch::nn::Conv2d(torch::nn::Conv2dOptions(boxChannels, outputChannels, 1)));
}

torch::Tensor DetectBoxBranchImpl::forward(const torch::Tensor &input)
{
    return m_output->forward(m_second->forward(m_first->forward(input)));
}

void DetectBoxBranchImpl::initializeBias(const double value)
{
    torch::NoGradGuard noGrad;
    m_output->bias.fill_(value);
}

DetectClassBranchImpl::DetectClassBranchImpl(const int64_t inputChannels,
                                             const int64_t classChannels,
                                             const int64_t classCount)
{
    m_first = register_module("0",
                              torch::nn::Sequential(Conv(inputChannels, inputChannels, 3, 1, inputChannels),
                                                    Conv(inputChannels, classChannels, 1, 1)));
    m_second = register_module("1",
                               torch::nn::Sequential(Conv(classChannels, classChannels, 3, 1, classChannels),
                                                     Conv(classChannels, classChannels, 1, 1)));
    m_output = register_module("2", torch::nn::Conv2d(torch::nn::Conv2dOptions(classChannels, classCount, 1)));
}

torch::Tensor DetectClassBranchImpl::forward(const torch::Tensor &input)
{
    return m_output->forward(m_second->forward(m_first->forward(input)));
}

void DetectClassBranchImpl::initializeBias(const double value)
{
    torch::NoGradGuard noGrad;
    m_output->bias.fill_(value);
}

DetectImpl::DetectImpl(const int64_t classCount, const std::vector<int64_t> &inputChannels) : m_classCount(classCount)
{
    const int64_t boxChannels = std::max<int64_t>({16, inputChannels.front() / 4, 64});
    const int64_t classChannels = std::max<int64_t>(inputChannels.front(), std::min<int64_t>(classCount, 100));
    m_cv2 = register_module("cv2", torch::nn::ModuleList());
    m_cv3 = register_module("cv3", torch::nn::ModuleList());
    for (const int64_t channels : inputChannels)
    {
        m_cv2->push_back(DetectBoxBranch(channels, boxChannels, 4 * m_regMax));

        m_cv3->push_back(DetectClassBranch(channels, classChannels, classCount));
    }
    m_dfl = register_module("dfl", DFL(m_regMax));

    constexpr std::array<double, 3> strides{8.0, 16.0, 32.0};
    for (size_t index = 0; index < strides.size(); ++index)
    {
        m_cv2->at<DetectBoxBranchImpl>(index).initializeBias(1.0);
        const double classBias =
            std::log(5.0 / static_cast<double>(classCount) / std::pow(640.0 / strides[index], 2.0));
        m_cv3->at<DetectClassBranchImpl>(index).initializeBias(classBias);
    }
}

std::vector<torch::Tensor> DetectImpl::forward(const std::vector<torch::Tensor> &input)
{
    std::vector<torch::Tensor> outputs;
    outputs.reserve(input.size());
    for (size_t index = 0; index < input.size(); ++index)
    {
        outputs.push_back(torch::cat({m_cv2->at<DetectBoxBranchImpl>(index).forward(input[index]),
                                      m_cv3->at<DetectClassBranchImpl>(index).forward(input[index])},
                                     1));
    }
    return outputs;
}

Yolo11NetworkImpl::Yolo11NetworkImpl(const int64_t classCount) : m_classCount(classCount)
{
    m_model = register_module("model", torch::nn::ModuleList());
    m_model->push_back(Conv(3, 16, 3, 2));
    m_model->push_back(Conv(16, 32, 3, 2));
    m_model->push_back(C3k2(32, 64, 1, false, true, 1, 0.25));
    m_model->push_back(Conv(64, 64, 3, 2));
    m_model->push_back(C3k2(64, 128, 1, false, true, 1, 0.25));
    m_model->push_back(Conv(128, 128, 3, 2));
    m_model->push_back(C3k2(128, 128, 1, true, true, 1, 0.5));
    m_model->push_back(Conv(128, 256, 3, 2));
    m_model->push_back(C3k2(256, 256, 1, true, true, 1, 0.5));
    m_model->push_back(SPPF(256, 256, 5));
    m_model->push_back(C2PSA(256, 256, 1, 0.5));
    m_model->push_back(torch::nn::Identity());
    m_model->push_back(torch::nn::Identity());
    m_model->push_back(C3k2(384, 128, 1, false, true, 1, 0.5));
    m_model->push_back(torch::nn::Identity());
    m_model->push_back(torch::nn::Identity());
    m_model->push_back(C3k2(256, 64, 1, false, true, 1, 0.5));
    m_model->push_back(Conv(64, 64, 3, 2));
    m_model->push_back(torch::nn::Identity());
    m_model->push_back(C3k2(192, 128, 1, false, true, 1, 0.5));
    m_model->push_back(Conv(128, 128, 3, 2));
    m_model->push_back(torch::nn::Identity());
    m_model->push_back(C3k2(384, 256, 1, true, true, 1, 0.5));
    m_model->push_back(Detect(classCount, std::vector<int64_t>{64, 128, 256}));
}

std::vector<torch::Tensor> Yolo11NetworkImpl::forward(const torch::Tensor &input)
{
    const torch::Tensor x0 = m_model->at<ConvImpl>(0).forward(input);
    const torch::Tensor x1 = m_model->at<ConvImpl>(1).forward(x0);
    const torch::Tensor x2 = m_model->at<C3k2Impl>(2).forward(x1);
    const torch::Tensor x3 = m_model->at<ConvImpl>(3).forward(x2);
    const torch::Tensor x4 = m_model->at<C3k2Impl>(4).forward(x3);
    const torch::Tensor x5 = m_model->at<ConvImpl>(5).forward(x4);
    const torch::Tensor x6 = m_model->at<C3k2Impl>(6).forward(x5);
    const torch::Tensor x7 = m_model->at<ConvImpl>(7).forward(x6);
    const torch::Tensor x8 = m_model->at<C3k2Impl>(8).forward(x7);
    const torch::Tensor x9 = m_model->at<SPPFImpl>(9).forward(x8);
    const torch::Tensor x10 = m_model->at<C2PSAImpl>(10).forward(x9);
    const torch::Tensor x11 = torch::upsample_nearest2d(x10, {x6.size(2), x6.size(3)}, std::nullopt, std::nullopt);
    const torch::Tensor x13 = m_model->at<C3k2Impl>(13).forward(torch::cat({x11, x6}, 1));
    const torch::Tensor x14 = torch::upsample_nearest2d(x13, {x4.size(2), x4.size(3)}, std::nullopt, std::nullopt);
    const torch::Tensor x16 = m_model->at<C3k2Impl>(16).forward(torch::cat({x14, x4}, 1));
    const torch::Tensor x17 = m_model->at<ConvImpl>(17).forward(x16);
    const torch::Tensor x19 = m_model->at<C3k2Impl>(19).forward(torch::cat({x17, x13}, 1));
    const torch::Tensor x20 = m_model->at<ConvImpl>(20).forward(x19);
    const torch::Tensor x22 = m_model->at<C3k2Impl>(22).forward(torch::cat({x20, x10}, 1));
    return m_model->at<DetectImpl>(23).forward({x16, x19, x22});
}

int64_t Yolo11NetworkImpl::classCount() const
{
    return m_classCount;
}
} // namespace visionaiflow::yolov11
