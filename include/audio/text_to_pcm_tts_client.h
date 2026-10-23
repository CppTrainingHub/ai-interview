#pragma once

#include <memory>
#include <string>
#include <vector>

#include "audio/audio_models.h"
#include "core/interview_ports.h"

namespace interview {
namespace audio {

/**
 * @brief 将文本编码为静音 PCM 占位音频的 TTS 实现
 * @note 该实现用于测试与离线后端流程，真实 TTS 后续可通过同一接口替换。
 */
class TextToPcmTTSClient final : public core::TTSClient {
public:
    /**
     * @brief 创建离线 TTS 客户端
     * @param format 输出 PCM 格式
     * @param milliseconds_per_character 每个字符对应的静音时长，必须为正数
     */
    explicit TextToPcmTTSClient(AudioFormat format = {},
                                int milliseconds_per_character = 40);

    ~TextToPcmTTSClient() override;

    TextToPcmTTSClient(const TextToPcmTTSClient&) = delete;
    TextToPcmTTSClient& operator=(const TextToPcmTTSClient&) = delete;

    TextToPcmTTSClient(TextToPcmTTSClient&&) noexcept;
    TextToPcmTTSClient& operator=(TextToPcmTTSClient&&) noexcept;

    /**
     * @brief 将文本合成为确定长度的静音 PCM
     * @param text 待合成文本
     * @return PCM 音频字节；空文本返回空数组
     */
    PcmAudio Synthesize(const std::string& text) override;

    /**
     * @brief 获取输出音频格式
     * @return PCM 格式描述
     */
    AudioFormat Format() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace audio
} // namespace interview
