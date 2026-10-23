#pragma once

#include <functional>
#include <memory>
#include <string>

#include "audio/speech_pipeline.h"

namespace interview {
namespace audio {

/** @brief 异步语音控制器的运行状态。 */
enum class AsyncSpeechState {
    kIdle,
    kCapturing,
    kRecognizing,
    kSpeaking,
    kStopping,
    kError
};

/**
 * @brief 在后台线程执行多块录音、ASR 和 TTS 播放。
 * @note 回调在工作线程执行；GUI 适配器必须将结果投递回 UI 主线程。
 */
class AsyncSpeechController {
public:
    using TranscriptCallback = std::function<void(const std::string&)>;
    using CompletionCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using StateCallback = std::function<void(AsyncSpeechState)>;

    explicit AsyncSpeechController(SpeechPipeline& pipeline);
    ~AsyncSpeechController();

    AsyncSpeechController(const AsyncSpeechController&) = delete;
    AsyncSpeechController& operator=(const AsyncSpeechController&) = delete;

    /**
     * @brief 启动后台录音，直到 RequestStopCapture 被调用。
     * @param on_transcript ASR 完成后的文本回调。
     * @param on_error 异步失败回调。
     */
    void StartCapture(TranscriptCallback on_transcript, ErrorCallback on_error = {});

    /** @brief 请求结束录音；该方法不阻塞调用线程。 */
    void RequestStopCapture();

    /**
     * @brief 异步合成并播放文本。
     * @param text 待朗读文本。
     * @param on_complete 播放完成回调。
     * @param on_error 异步失败回调。
     */
    void Speak(std::string text,
               CompletionCallback on_complete = {},
               ErrorCallback on_error = {});

    /** @brief 等待当前后台操作结束。 */
    void Wait();

    /** @brief 设置状态变化回调。 */
    void SetStateCallback(StateCallback callback);

    /** @brief 返回当前控制器状态。 */
    AsyncSpeechState State() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace audio
} // namespace interview
