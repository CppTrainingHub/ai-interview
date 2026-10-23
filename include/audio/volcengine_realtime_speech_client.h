#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "audio/audio_models.h"
#include "core/interview_ports.h"

namespace interview {
namespace audio {

/**
 * @brief 火山引擎实时语音配置
 * @note 认证字段应来自环境变量或本地配置文件，不应硬编码到源码或提交到仓库
 */
struct VolcengineRealtimeSpeechConfig {
    std::string websocket_url =
        "wss://openspeech.bytedance.com/api/v3/realtime/dialogue";
    std::map<std::string, std::string> headers;

    AudioFormat input_format;
    AudioFormat output_format;

    std::string bot_name = "AI 面试官";
    std::string system_role = "你是一位专业的技术面试官，只负责提问、追问和简短反馈。";
    std::string speaking_style = "简洁、清晰、直接。";
    std::string city = "北京";
    std::string speaker = "zh_male_yunzhou_jupiter_bigtts";
    std::string audio_format_name = "pcm";
    std::string input_mode = "audio";
    std::string audit_response = "当前内容无法继续。";
    bool strict_audit = false;
    int recv_timeout_seconds = 10;
    int asr_end_smooth_window_ms = 2000;
    int asr_vad_silence_duration_ms = 2500;
    int asr_vad_speech_trigger_duration_ms = 400;
    int operation_timeout_ms = 30000;

    /**
     * @brief 生成 StartSession 请求 JSON
     * @return 与实时对话服务协议匹配的 JSON payload
     */
    nlohmann::json BuildStartSessionPayload() const;
};

/**
 * @brief 基于火山引擎实时对话 WebSocket 的 ASR/TTS 客户端
 * @note 该类同时实现 ASRClient 与 TTSClient；网络和协议细节隐藏在 PImpl 中
 */
class VolcengineRealtimeSpeechClient final : public core::ASRClient,
                                             public core::TTSClient {
public:
    /**
     * @brief 创建实时语音客户端
     * @param config 服务地址、认证头、音频格式和 ASR/TTS 参数
     */
    explicit VolcengineRealtimeSpeechClient(VolcengineRealtimeSpeechConfig config);

    ~VolcengineRealtimeSpeechClient() override;

    VolcengineRealtimeSpeechClient(const VolcengineRealtimeSpeechClient&) = delete;
    VolcengineRealtimeSpeechClient& operator=(const VolcengineRealtimeSpeechClient&) = delete;

    VolcengineRealtimeSpeechClient(VolcengineRealtimeSpeechClient&&) noexcept;
    VolcengineRealtimeSpeechClient& operator=(VolcengineRealtimeSpeechClient&&) noexcept;

    /**
     * @brief 主动建立 WebSocket 连接并初始化会话
     * @throws std::runtime_error 当认证、网络或会话初始化失败时抛出
     */
    void Connect();

    /**
     * @brief 关闭实时语音会话和 WebSocket 连接
     * @note 可重复调用，析构函数会自动调用
     */
    void Close();

    /**
     * @brief 将输入 PCM 音频发送给实时服务并等待最终 ASR 文本
     * @param pcm_audio 原始 16-bit little-endian PCM 音频字节
     * @return 最终识别文本；超时前没有最终结果时返回空字符串
     */
    std::string Transcribe(const PcmAudio& pcm_audio) override;

    /**
     * @brief 将文本发送给实时服务进行 TTS 并收集返回的 PCM 音频
     * @param text 待朗读文本
     * @return 服务端返回的 PCM 音频字节
     */
    PcmAudio Synthesize(const std::string& text) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace audio
} // namespace interview
