/**
 * @file realtime_client.h
 * @brief 实时对话WebSocket客户端模块
 *
 * 与字节跳动实时对话服务通信的WebSocket客户端类。
 * 负责建立安全连接、发送音频数据、接收服务器响应等。
 */

#pragma once

#include <string>
#include <memory>
#include <functional>
#include "common/protocol.h"

namespace interview {
namespace services {

/**
 * @brief 实时对话WebSocket客户端类
 *
 * 封装与字节跳动实时对话服务的所有WebSocket通信逻辑。
 * 提供高层次的接口用于音频传输和文本查询。
 *
 * 使用示例：
 * @code
 * // 创建客户端
 * std::string url = "wss://openspeech.bytedance.com/api/v3/realtime/dialogue";
 * std::map<std::string, std::string> headers;
 * headers["X-Api-App-ID"] = "your-app-id";
 * headers["X-Api-Access-Key"] = "your-key";
 *
 * services::RealtimeClient client(url, headers);
 *
 * // 设置响应回调
 * client.SetResponseCallback([](const common::ParsedResponse& resp) {
 *     if (resp.event == common::events::TTS_START) {
 *         std::cout << "AI started speaking" << std::endl;
 *     }
 * });
 *
 * // 连接服务器（自动完成START_CONNECTION和START_SESSION）
 * client.Connect();
 *
 * // 发送音频数据
 * std::vector<uint8_t> audio_data = ReadMicrophone();
 * client.SendAudioData(audio_data);
 *
 * // 发送文本查询
 * client.SendTextQuery("请问第一个问题");
 *
 * // 关闭连接
 * client.Close();
 * @endcode
 */
class RealtimeClient {
public:
    /// @brief 响应回调函数类型，用于接收服务器事件
    using ResponseCallback = std::function<void(const common::ParsedResponse&)>;
    /**
     * @brief 构造函数
     *
     * 创建WebSocket客户端实例，解析URL和配置。
     *
     * @param url WebSocket服务器URL（wss://开头）
     * @param headers HTTP请求头，包含认证信息（X-Api-*）
     */
    RealtimeClient(const std::string& url,
                   const std::map<std::string, std::string>& headers);

    /**
     * @brief 析构函数
     *
     * 自动关闭连接，清理资源。
     */
    ~RealtimeClient();

    /**
     * @brief 连接到服务器
     *
     * 建立WebSocket连接并完成会话初始化。
     *
     * @throws std::runtime_error 连接失败、握手失败、初始化失败
     */
    void Connect();

    /**
     * @brief 发送音频数据
     *
     * 发送麦克风录音的PCM音频数据给服务器进行ASR识别。
     *
     * @param audio 原始PCM音频数据（每个采样点2字节，小端序）
     * @throws std::runtime_error 未连接或发送失败
     */
    void SendAudioData(const std::vector<uint8_t>& audio);

    /**
     * @brief 发送文本查询
     *
     * 发送文本提示让AI主动说话（而不是等待用户说话触发）。
     *
     * @param text 文本提示内容（中文）
     * @throws std::runtime_error 未连接或发送失败
     */
    void SendTextQuery(const std::string& text);

    /**
     * @brief 接收服务器响应（阻塞）
     *
     * 同步接收一条服务器消息并解析。
     * 此方法主要用于接收线程，外部通常使用回调机制。
     *
     * @return 解析后的响应结构
     * @throws std::runtime_error 未连接或接收失败
     */
    common::ParsedResponse ReceiveResponse();

    /**
     * @brief 设置响应回调函数
     *
     * 注册回调函数用于接收服务器事件。
     * 回调在接收线程中执行，需要保证线程安全。
     *
     * 回调时机：
     * - TTS_START: AI开始说话
     * - TTS_END: AI说完了
     * - USER_START_SPEAKING: 检测到用户开始说话
     * - USER_STOP_SPEAKING: 用户说完了
     * - ASR_RESULT: 语音识别结果
     * - SESSION_FINISHED: 会话正常结束
     * - SERVER_ACK with audio: TTS音频数据
     *
     * @param callback 响应回调函数
     */
    void SetResponseCallback(ResponseCallback callback);

    /**
     * @brief 关闭连接
     *
     * 停止接收线程，发送结束请求，关闭WebSocket连接。
     *
     * 关闭流程：
     * 1. 停止接收线程
     * 2. 发送FINISH_SESSION请求
     * 3. 发送FINISH_CONNECTION请求
     * 4. 关闭WebSocket（发送Close帧）
     * 5. 等待接收线程退出
     *
     * 注意：
     * - 可重复调用（内部有状态检查）
     * - 不会抛出异常（错误只记录日志）
     */
    void Close();

private:
    class RealtimeClientImpl;
    std::unique_ptr<RealtimeClientImpl> pimpl_;
};

} // namespace services
} // namespace interview
