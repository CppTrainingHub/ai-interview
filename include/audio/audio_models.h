#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace interview {
namespace audio {

/**
 * @brief PCM 采样格式
 * @note 优先支持 16-bit little-endian PCM，后续可扩展 float32 或压缩格式
 */
enum class SampleFormat {
    kPcmS16Le
};

/**
 * @brief 音频流格式描述
 * @note 该结构只描述字节含义，不持有音频数据，也不绑定设备或第三方库
 */
struct AudioFormat {
    int sample_rate_hz = 16000;
    int channels = 1;
    SampleFormat sample_format = SampleFormat::kPcmS16Le;

    /**
     * @brief 返回单个采样点占用字节数
     * @return 每个采样点的字节数
     */
    std::size_t BytesPerSample() const;

    /**
     * @brief 返回每一帧音频占用字节数
     * @return 单帧字节数，等于 channels * BytesPerSample()
     */
    std::size_t BytesPerFrame() const;
};

/**
 * @brief 一段 PCM 音频数据
 * @note audio 模块使用 unsigned char 存储原始字节，避免 public header 暴露平台音频类型
 */
struct PcmAudio {
    AudioFormat format;
    std::basic_string<unsigned char> bytes;
};

} // namespace audio
} // namespace interview
