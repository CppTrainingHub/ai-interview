#pragma once

#include <memory>
#include <string>

#include "audio/audio_device.h"

namespace interview {
namespace audio {

/**
 * @brief 基于 WAV 文件的音频设备
 * @note 输入从 WAV 文件分块读取，输出在 Close 时写入 WAV 文件，适合离线调试完整音频链路。
 */
class WavAudioDevice final : public AudioDevice {
public:
    /**
     * @brief 创建 WAV 文件音频设备
     * @param input_path 输入 WAV 文件路径；为空时输入返回静音
     * @param output_path 输出 WAV 文件路径；为空时只在内存中记录输出
     * @param config 音频设备配置
     */
    WavAudioDevice(std::string input_path,
                   std::string output_path,
                   AudioDeviceConfig config = {});

    ~WavAudioDevice() override;

    WavAudioDevice(const WavAudioDevice&) = delete;
    WavAudioDevice& operator=(const WavAudioDevice&) = delete;

    WavAudioDevice(WavAudioDevice&&) noexcept;
    WavAudioDevice& operator=(WavAudioDevice&&) noexcept;

    void OpenInput() override;
    void OpenOutput() override;
    std::vector<unsigned char> ReadInput() override;
    void WriteOutput(const std::vector<unsigned char>& pcm_audio) override;
    void Close() override;
    AudioDeviceConfig Config() const override;

    /**
     * @brief 获取已经写入输出端但尚未落盘或已经落盘的 PCM 字节
     * @return 输出 PCM 字节
     */
    std::vector<unsigned char> WrittenOutput() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace audio
} // namespace interview
