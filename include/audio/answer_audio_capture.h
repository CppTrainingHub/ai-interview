#pragma once

#include <memory>

#include "audio/audio_device.h"
#include "audio/audio_models.h"

namespace interview {
namespace audio {

/**
 * @brief 按块收集一次候选人回答的 PCM 音频。
 * @note 本类不创建线程，也不判断语音结束；GUI 或异步控制器决定何时开始和停止。
 */
class AnswerAudioCapture {
public:
    /**
     * @brief 创建回答录音器。
     * @param device 已打开输入端的音频设备，生命周期必须长于本对象。
     */
    explicit AnswerAudioCapture(AudioDevice& device);
    ~AnswerAudioCapture();

    AnswerAudioCapture(const AnswerAudioCapture&) = delete;
    AnswerAudioCapture& operator=(const AnswerAudioCapture&) = delete;
    AnswerAudioCapture(AnswerAudioCapture&&) noexcept;
    AnswerAudioCapture& operator=(AnswerAudioCapture&&) noexcept;

    /** @brief 开始一次新的录音并清除上次数据。 */
    void Begin();

    /** @brief 阻塞读取并追加一个设备音频块。 */
    void CaptureNextChunk();

    /**
     * @brief 完成录音并返回合并后的 PCM。
     * @return 带输入格式信息的完整回答音频。
     */
    PcmAudio Finish();

    /** @brief 取消录音并丢弃已收集数据。 */
    void Cancel();

    /** @brief 查询当前是否正在录音。 */
    bool IsCapturing() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace audio
} // namespace interview
