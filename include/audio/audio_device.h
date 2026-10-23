#pragma once

#include <vector>

#include "audio/audio_models.h"

namespace interview {
namespace audio {

/**
 * @brief 音频设备配置
 * @note 只描述输入输出 PCM 格式和每次阻塞读写的帧数，不绑定具体设备库。
 */
struct AudioDeviceConfig {
    AudioFormat input_format;
    AudioFormat output_format;
    int input_frames_per_buffer = 3200;
    int output_frames_per_buffer = 3200;
};

/**
 * @brief 音频输入输出设备接口
 * @note 真实麦克风、扬声器、WAV 文件、测试空设备都可以实现该接口。
 */
class AudioDevice {
public:
    virtual ~AudioDevice() = default;

    /**
     * @brief 打开输入流
     * @throws std::runtime_error 当设备不可用或格式不支持时抛出
     */
    virtual void OpenInput() = 0;

    /**
     * @brief 打开输出流
     * @throws std::runtime_error 当设备不可用或格式不支持时抛出
     */
    virtual void OpenOutput() = 0;

    /**
     * @brief 阻塞读取一段输入 PCM 音频
     * @return 原始 PCM 字节
     * @throws std::runtime_error 当输入流未打开或读取失败时抛出
     */
    virtual std::vector<unsigned char> ReadInput() = 0;

    /**
     * @brief 阻塞写入一段输出 PCM 音频
     * @param pcm_audio 原始 PCM 字节
     * @throws std::runtime_error 当输出流未打开、音频未对齐或写入失败时抛出
     */
    virtual void WriteOutput(const std::vector<unsigned char>& pcm_audio) = 0;

    /**
     * @brief 关闭输入输出流并释放资源
     * @note 可重复调用。
     */
    virtual void Close() = 0;

    /**
     * @brief 获取设备配置
     * @return 音频设备配置
     */
    virtual AudioDeviceConfig Config() const = 0;
};

} // namespace audio
} // namespace interview
