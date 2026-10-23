#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace interview {
namespace audio {

/**
 * @brief 火山引擎实时对话二进制协议事件常量
 * @note 该命名空间只保存协议编号，不包含网络连接或业务流程逻辑
 */
namespace realtime_events {
    constexpr std::uint32_t kStartConnection = 1;
    constexpr std::uint32_t kFinishConnection = 2;
    constexpr std::uint32_t kConnectionStarted = 50;
    constexpr std::uint32_t kConnectionFailed = 51;
    constexpr std::uint32_t kConnectionFinished = 52;
    constexpr std::uint32_t kStartSession = 100;
    constexpr std::uint32_t kFinishSession = 102;
    constexpr std::uint32_t kSessionFinished = 152;
    constexpr std::uint32_t kSessionEnded = 153;
    constexpr std::uint32_t kTaskRequest = 200;
    constexpr std::uint32_t kTtsStart = 350;
    constexpr std::uint32_t kTtsEnd = 359;
    constexpr std::uint32_t kUserStartSpeaking = 450;
    constexpr std::uint32_t kAsrResult = 451;
    constexpr std::uint32_t kUserStopSpeaking = 459;
    constexpr std::uint32_t kChatTextQuery = 501;
} // namespace realtime_events

/**
 * @brief 火山引擎实时对话响应解析结果
 * @note JSON payload 和二进制音频 payload 互斥；is_binary 为 true 时读取 payload_bytes
 */
struct RealtimeSpeechResponse {
    std::string message_type;
    std::uint32_t event = 0;
    std::string session_id;
    std::string connect_id;
    nlohmann::json payload;
    std::vector<unsigned char> payload_bytes;
    std::uint32_t payload_size = 0;
    std::uint32_t error_code = 0;
    bool is_binary = false;
};

/**
 * @brief 构建 JSON 控制请求
 * @param event 协议事件编号
 * @param session_id 会话 ID；连接级事件可为空
 * @param payload JSON 请求体
 * @return 可直接通过 WebSocket 发送的二进制消息
 */
std::vector<unsigned char> BuildRealtimeFullRequest(std::uint32_t event,
                                                    const std::string& session_id,
                                                    const nlohmann::json& payload);

/**
 * @brief 构建音频流请求
 * @param event 协议事件编号，通常为 realtime_events::kTaskRequest
 * @param session_id 会话 ID
 * @param audio_data PCM 音频字节
 * @return 可直接通过 WebSocket 发送的二进制消息
 */
std::vector<unsigned char> BuildRealtimeAudioRequest(
    std::uint32_t event,
    const std::string& session_id,
    const std::vector<unsigned char>& audio_data);

/**
 * @brief 解析服务器响应
 * @param data WebSocket 收到的完整二进制消息
 * @return 解析后的响应对象
 * @throws std::runtime_error 当协议结构非法或压缩格式暂不支持时抛出
 */
RealtimeSpeechResponse ParseRealtimeSpeechResponse(const std::vector<unsigned char>& data);

/**
 * @brief 从 ASR_RESULT payload 中提取最终识别文本
 * @param payload 服务端 JSON payload
 * @return 若 payload 包含最终识别结果则返回文本，否则返回空字符串
 */
std::string ExtractFinalAsrText(const nlohmann::json& payload);

} // namespace audio
} // namespace interview
