#pragma once

#include <memory>

#include "audio/audio_device.h"

namespace interview {
namespace audio {

/**
 * @brief 基于 PortAudio 的真实麦克风/扬声器设备
 * @note 该类的实现只在 CMake 开启 AI_INTERVIEW_ENABLE_PORTAUDIO 时参与编译。
 */
class PortAudioAudioDevice final : public AudioDevice {
public:
    /**
     * @brief 创建 PortAudio 设备适配器
     * @param config 输入输出音频配置
     */
    explicit PortAudioAudioDevice(AudioDeviceConfig config = {});

    ~PortAudioAudioDevice() override;

    PortAudioAudioDevice(const PortAudioAudioDevice&) = delete;
    PortAudioAudioDevice& operator=(const PortAudioAudioDevice&) = delete;

    PortAudioAudioDevice(PortAudioAudioDevice&&) noexcept;
    PortAudioAudioDevice& operator=(PortAudioAudioDevice&&) noexcept;

    void OpenInput() override;
    void OpenOutput() override;
    std::vector<unsigned char> ReadInput() override;
    void WriteOutput(const std::vector<unsigned char>& pcm_audio) override;
    void Close() override;
    AudioDeviceConfig Config() const override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace audio
} // namespace interview
