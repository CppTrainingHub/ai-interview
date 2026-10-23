#pragma once

#include <memory>
#include <string>

#include "audio/audio_device.h"
#include "core/interview_ports.h"

namespace interview {
namespace audio {

/**
 * @brief 语音链路编排器
 * @note 将音频设备、ASR 和 TTS 串成完整链路，不直接依赖具体硬件或云厂商。
 */
class SpeechPipeline {
public:
    /**
     * @brief 创建语音链路
     * @param device 音频输入输出设备，调用者负责保证生命周期长于本对象
     * @param asr 语音识别客户端，调用者负责保证生命周期长于本对象
     * @param tts 语音合成客户端，调用者负责保证生命周期长于本对象
     */
    SpeechPipeline(AudioDevice& device,
                   core::ASRClient& asr,
                   core::TTSClient& tts);

    ~SpeechPipeline();

    SpeechPipeline(const SpeechPipeline&) = delete;
    SpeechPipeline& operator=(const SpeechPipeline&) = delete;

    SpeechPipeline(SpeechPipeline&&) noexcept;
    SpeechPipeline& operator=(SpeechPipeline&&) noexcept;

    /**
     * @brief 打开音频输入和输出
     * @throws std::runtime_error 当设备打开失败时抛出
     */
    void Start();

    /**
     * @brief 读取一段音频并识别为文本
     * @return ASR 识别文本
     */
    std::string CaptureAnswerText();

    /** @brief 开始手动控制的一次回答录音。 */
    void BeginAnswerCapture();

    /** @brief 读取并追加一个回答音频块。 */
    void CaptureAnswerChunk();

    /** @brief 停止录音、执行 ASR 并返回文本。 */
    std::string FinishAnswerCapture();

    /** @brief 取消当前回答录音。 */
    void CancelAnswerCapture();

    /** @brief 查询语音链路是否已启动。 */
    bool IsStarted() const;

    /**
     * @brief 将文本合成为音频并写入输出设备
     * @param text 待朗读文本
     */
    void SpeakText(const std::string& text);

    /**
     * @brief 关闭音频设备
     */
    void Stop();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace audio
} // namespace interview
