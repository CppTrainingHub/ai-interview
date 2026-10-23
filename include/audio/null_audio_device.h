#pragma once

#include <memory>
#include <vector>

#include "audio/audio_device.h"

namespace interview {
namespace audio {

/**
 * @brief 不访问真实硬件的音频设备
 * @note 用于单元测试、CI、无麦克风环境和本地链路调试；输入可预置，输出会被记录
 */
class NullAudioDevice final : public AudioDevice {
public:
    /**
     * @brief 创建空音频设备
     * @param config 音频设备配置
     */
    explicit NullAudioDevice(AudioDeviceConfig config = {});

    ~NullAudioDevice() override;

    NullAudioDevice(const NullAudioDevice&) = delete;
    NullAudioDevice& operator=(const NullAudioDevice&) = delete;

    NullAudioDevice(NullAudioDevice&&) noexcept;
    NullAudioDevice& operator=(NullAudioDevice&&) noexcept;

    void OpenInput() override;
    void OpenOutput() override;
    std::vector<unsigned char> ReadInput() override;
    void WriteOutput(const std::vector<unsigned char>& pcm_audio) override;
    void Close() override;
    AudioDeviceConfig Config() const override;

    /**
     * @brief 预置一段输入音频，下一次 ReadInput 会返回它
     * @param pcm_audio 原始 PCM 字节
     */
    void QueueInput(const std::vector<unsigned char>& pcm_audio);

    /**
     * @brief 获取所有已经写入输出端的 PCM 字节
     * @return 输出 PCM 字节
     */
    std::vector<unsigned char> WrittenOutput() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace audio
} // namespace interview
