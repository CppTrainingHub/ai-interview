#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <nlohmann/json.hpp>

namespace interview {
namespace common {

/**
 * @brief WebSocket通信协议定义
 *
 * 实现了与字节跳动实时对话服务的二进制协议通信
 * 协议格式：[头部 | payload大小 | payload数据]
 */

// 协议版本号（4位）
constexpr uint8_t PROTOCOL_VERSION = 0b0001;
// 默认头部大小（4位）
constexpr uint8_t DEFAULT_HEADER_SIZE = 0b0001;

/**
 * @brief 事件常量定义
 *
 * 定义了客户端和服务器之间通信的所有事件类型
 * 事件分为：连接控制、会话控制、任务查询、TTS、ASR等类别
 */
namespace events {
    // 连接控制事件（1-99）
    constexpr uint32_t START_CONNECTION = 1;        // 客户端->服务器：开始连接
    constexpr uint32_t FINISH_CONNECTION = 2;       // 客户端->服务器：结束连接
    constexpr uint32_t CONNECTION_STARTED = 50;     // 服务器->客户端：连接已建立
    constexpr uint32_t CONNECTION_FAILED = 51;      // 服务器->客户端：连接失败
    constexpr uint32_t CONNECTION_FINISHED = 52;    // 服务器->客户端：连接已结束

    // 会话控制事件（100-199）
    constexpr uint32_t START_SESSION = 100;         // 客户端->服务器：开始会话
    constexpr uint32_t FINISH_SESSION = 102;        // 客户端->服务器：结束会话
    constexpr uint32_t SESSION_FINISHED = 152;      // 服务器->客户端：会话已结束
    constexpr uint32_t SESSION_ENDED = 153;         // 服务器->客户端：会话已终止

    // 任务和查询事件（200-599）
    constexpr uint32_t TASK_REQUEST = 200;          // 客户端->服务器：任务请求
    constexpr uint32_t CHAT_TEXT_QUERY = 501;       // 客户端->服务器：文本对话查询

    // TTS（文本转语音）事件（350-399）
    constexpr uint32_t TTS_START = 350;             // 服务器->客户端：TTS开始播放
    constexpr uint32_t TTS_END = 359;               // 服务器->客户端：TTS播放结束

    // ASR（语音识别）事件（450-499）
    constexpr uint32_t USER_START_SPEAKING = 450;   // 服务器->客户端：检测到用户开始说话
    constexpr uint32_t ASR_RESULT = 451;            // 服务器->客户端：ASR识别结果
    constexpr uint32_t USER_STOP_SPEAKING = 459;    // 服务器->客户端：检测到用户停止说话
}

/**
 * @brief 消息类型枚举
 *
 * 定义了协议中支持的消息类型
 * - 客户端消息：完整请求、仅音频请求
 * - 服务器消息：完整响应、确认、错误响应
 */
enum class MessageType : uint8_t {
    CLIENT_FULL_REQUEST = 0b0001,       // 客户端完整请求（包含JSON payload）
    CLIENT_AUDIO_ONLY_REQUEST = 0b0010, // 客户端仅音频请求（音频流数据）
    SERVER_FULL_RESPONSE = 0b1001,      // 服务器完整响应（包含JSON payload）
    SERVER_ACK = 0b1011,                // 服务器确认消息（可能包含音频数据）
    SERVER_ERROR_RESPONSE = 0b1111      // 服务器错误响应
};

/**
 * @brief 消息标志位枚举
 *
 * 控制消息中包含的可选字段
 * - 序列号标志：用于标识消息顺序
 * - 事件标志：标识消息是否包含事件类型
 */
enum class MessageFlags : uint8_t {
    NO_SEQUENCE = 0b0000,               // 不包含序列号
    POS_SEQUENCE = 0b0001,              // 包含正序列号
    NEG_SEQUENCE = 0b0010,              // 包含负序列号
    MSG_WITH_EVENT = 0b0100             // 消息包含事件类型
};

/**
 * @brief 序列化方法枚举
 *
 * 定义payload的序列化格式
 */
enum class SerializationMethod : uint8_t {
    NO_SERIALIZATION = 0b0000,          // 无序列化（原始二进制数据）
    JSON = 0b0001,                      // JSON格式
    THRIFT = 0b0011,                    // Thrift格式
    CUSTOM_TYPE = 0b1111                // 自定义格式
};

/**
 * @brief 压缩类型枚举
 *
 * 定义payload的压缩算法
 */
enum class CompressionType : uint8_t {
    NO_COMPRESSION = 0b0000,            // 无压缩
    GZIP = 0b0001,                      // GZIP压缩
    CUSTOM_COMPRESSION = 0b1111         // 自定义压缩算法
};

/**
 * @brief 事件类型枚举（用于类型安全的事件处理）
 *
 * 这是events命名空间中常量的强类型版本，用于C++类型安全的事件处理。
 * 通过枚举类可以避免隐式转换，提高代码安全性。
 * 主要用于事件回调函数的参数类型，确保只处理已知的事件类型。
 */
enum class EventType : uint32_t {
    SESSION_FINISHED = 152,       // 会话正常结束（服务器主动结束）
    SESSION_ENDED = 153,          // 会话被终止（异常或强制终止）
    TTS_START = 350,              // TTS开始播放音频
    TTS_END = 359,                // TTS播放结束
    USER_START_SPEAKING = 450,    // 检测到用户开始说话（VAD触发）
    ASR_RESULT = 451,             // ASR识别结果（可能是中间结果或最终结果）
    USER_STOP_SPEAKING = 459      // 检测到用户停止说话（VAD结束）
};

/**
 * @brief 协议头结构（6字节固定长度）
 *
 * 协议头布局：
 * Byte 0: [version(4bit) | header_size(4bit)]
 * Byte 1: message_type
 * Byte 2: flags
 * Byte 3: serialization
 * Byte 4: compression
 * Byte 5: reserved
 */
struct ProtocolHeader {
    uint8_t version : 4;                // 协议版本号（4位）
    uint8_t header_size : 4;            // 头部大小（4位）
    MessageType message_type;           // 消息类型
    MessageFlags flags;                 // 消息标志
    SerializationMethod serialization;  // 序列化方法
    CompressionType compression;        // 压缩类型
    uint8_t reserved;                   // 保留字节
    std::vector<uint8_t> extensions;    // 扩展字段（可变长度）

    ProtocolHeader();
};

/**
 * @brief 服务器响应解析结果
 *
 * 包含从服务器接收到的消息中解析出的所有字段
 * 支持JSON payload和二进制音频数据两种类型
 */
struct ParsedResponse {
    std::string message_type;           // 消息类型字符串表示
    int32_t sequence = 0;               // 序列号（仅当flags包含序列号时有效）
    uint32_t event = 0;                 // 事件类型编号
    std::string session_id;             // 会话ID
    std::string connect_id;             // 连接ID（仅某些事件包含，如CONNECTION_STARTED）
    nlohmann::json payload;             // JSON格式的payload数据
    std::vector<uint8_t> payload_bytes; // 二进制格式的payload数据（用于音频流）
    uint32_t payload_size = 0;          // Payload大小（字节数）
    uint32_t code = 0;                  // 错误码（仅错误响应包含）
    bool is_binary = false;             // 标记payload是否为二进制数据

    ParsedResponse() = default;
};

/**
 * @brief 协议处理类
 *
 * 提供与字节跳动实时对话服务通信的二进制协议编解码功能。
 * 所有方法都是静态的，作为工具类使用，无需实例化。
 *
 * 主要功能：
 * 1. 协议头生成和解析
 * 2. 请求消息构建（完整请求、音频请求）
 * 3. 响应消息解析（完整响应、ACK、错误）
 * 4. GZIP压缩/解压缩
 * 5. 大端字节序转换
 *
 * 使用场景：
 * - 发送StartConnection/StartSession/FinishSession请求
 * - 发送音频流数据（麦克风录音）
 * - 发送文本查询请求
 * - 解析服务器返回的事件和音频数据
 */
class Protocol {
public:
    /**
     * @brief 生成协议头
     *
     * 生成4字节的协议头，包含版本号、消息类型、标志位、序列化方法和压缩类型。
     * 协议头布局：
     * - Byte 0: [version(4bit) | header_size(4bit)]
     * - Byte 1: [message_type(4bit) | flags(4bit)]
     * - Byte 2: [serialization(4bit) | compression(4bit)]
     * - Byte 3: reserved
     *
     * @param message_type 消息类型（客户端请求或服务器响应）
     * @param flags 消息标志（是否包含序列号、事件等）
     * @param serialization 序列化方法（JSON、Thrift等）
     * @param compression 压缩类型（无压缩、GZIP等）
     * @param reserved 保留字节，默认0x00
     * @return 4字节的协议头数据
     */
    static std::vector<uint8_t> GenerateHeader(
        MessageType message_type = MessageType::CLIENT_FULL_REQUEST,
        MessageFlags flags = MessageFlags::MSG_WITH_EVENT,
        SerializationMethod serialization = SerializationMethod::JSON,
        CompressionType compression = CompressionType::NO_COMPRESSION,
        uint8_t reserved = 0x00
    );

    /**
     * @brief 解析服务器响应
     *
     * 从接收到的二进制数据中解析出协议头、事件、payload等信息。
     * 支持多种消息类型：
     * - SERVER_FULL_RESPONSE: 包含JSON payload的完整响应（事件通知）
     * - SERVER_ACK: 确认消息，可能包含音频数据（TTS音频流）
     * - SERVER_ERROR_RESPONSE: 错误响应，包含错误码和错误消息
     *
     * 解析流程：
     * 1. 解析协议头（4字节）
     * 2. 根据flags解析序列号（可选）
     * 3. 根据flags解析事件编号（可选）
     * 4. 根据事件类型解析session_id（可选）
     * 5. 根据事件类型解析connect_id（可选）
     * 6. 解析payload大小和数据
     * 7. 根据压缩类型解压payload
     * 8. 根据序列化方法反序列化payload
     *
     * @param data 接收到的完整二进制数据
     * @return 解析后的响应结构，包含所有字段
     */
    static ParsedResponse ParseResponse(const std::vector<uint8_t>& data);

    /**
     * @brief GZIP压缩
     *
     * 使用zlib库进行GZIP格式压缩。
     * 压缩后的数据可以显著减少网络传输量，适用于大型JSON payload。
     *
     * @param data 待压缩的原始数据
     * @return 压缩后的GZIP数据
     * @throws std::runtime_error 压缩失败时抛出异常
     */
    static std::vector<uint8_t> CompressGzip(const std::vector<uint8_t>& data);

    /**
     * @brief GZIP解压
     *
     * 使用zlib库进行GZIP格式解压缩。
     * 服务器返回的payload如果使用了GZIP压缩，需要先解压才能反序列化。
     *
     * @param data 压缩的GZIP数据
     * @return 解压后的原始数据
     * @throws std::runtime_error 解压失败时抛出异常
     */
    static std::vector<uint8_t> DecompressGzip(const std::vector<uint8_t>& data);

    /**
     * @brief 构建完整请求消息
     *
     * 构建包含JSON payload的完整请求消息（CLIENT_FULL_REQUEST）。
     * 消息结构：
     * [协议头(4字节)] [event(4字节)] [session_id长度(4字节)] [session_id数据]
     * [payload大小(4字节)] [payload数据]
     *
     * 适用场景：
     * - START_CONNECTION: 建立WebSocket连接
     * - START_SESSION: 启动对话会话
     * - FINISH_SESSION: 结束对话会话
     * - FINISH_CONNECTION: 关闭WebSocket连接
     * - CHAT_TEXT_QUERY: 发送文本查询（让AI主动说话）
     *
     * @param event 事件编号（protocol::events命名空间中的常量）
     * @param session_id 会话ID（某些事件不需要，如START_CONNECTION）
     * @param payload JSON格式的payload数据
     * @return 完整的请求消息二进制数据
     */
    static std::vector<uint8_t> BuildFullRequest(
        uint32_t event,
        const std::string& session_id,
        const nlohmann::json& payload
    );

    /**
     * @brief 构建音频请求消息
     *
     * 构建包含音频数据的请求消息（CLIENT_AUDIO_ONLY_REQUEST）。
     * 消息结构：
     * [协议头(4字节)] [event(4字节)] [session_id长度(4字节)] [session_id数据]
     * [音频大小(4字节)] [音频数据]
     *
     * 适用场景：
     * - TASK_REQUEST: 发送麦克风录音的PCM音频流
     *
     * 注意事项：
     * - 音频格式必须与StartSession时配置的一致（默认16kHz单声道PCM）
     * - 音频数据不压缩，直接发送原始PCM字节
     * - 需要定时发送（如每10ms发送一次），保证实时性
     *
     * @param event 事件编号（通常是TASK_REQUEST）
     * @param session_id 会话ID
     * @param audio_data 原始PCM音频数据（int16格式）
     * @return 完整的音频请求消息二进制数据
     */
    static std::vector<uint8_t> BuildClientAudioRequest(
        uint32_t event,
        const std::string& session_id,
        const std::vector<uint8_t>& audio_data
    );

    /**
     * @brief 将uint32转为大端字节序
     *
     * 协议要求所有多字节整数使用大端字节序（网络字节序）。
     * 这个方法将uint32值转换为4个字节并追加到buffer中。
     *
     * 例如：0x12345678 -> [0x12, 0x34, 0x56, 0x78]
     *
     * @param buffer 目标缓冲区，转换后的4个字节会追加到末尾
     * @param value 待转换的uint32值
     */
    static void AppendUint32BigEndian(std::vector<uint8_t>& buffer, uint32_t value);

    /**
     * @brief 从大端字节序读取uint32
     *
     * 从4个字节的大端字节序数据中读取uint32值。
     * 例如：[0x12, 0x34, 0x56, 0x78] -> 0x12345678
     *
     * @param data 指向4字节大端数据的指针
     * @return 读取的uint32值
     */
    static uint32_t ReadUint32BigEndian(const uint8_t* data);

    /**
     * @brief 从大端字节序读取int32
     *
     * 从4个字节的大端字节序数据中读取int32值（有符号）。
     * 主要用于读取序列号（sequence），序列号可能是负数。
     *
     * @param data 指向4字节大端数据的指针
     * @return 读取的int32值（有符号）
     */
    static int32_t ReadInt32BigEndian(const uint8_t* data);

private:
    /**
     * @brief 检查flags中是否包含序列号
     *
     * 检查消息标志位中是否设置了POS_SEQUENCE或NEG_SEQUENCE位。
     * 如果包含序列号，需要在解析时读取4字节的序列号字段。
     *
     * @param message_flags 消息标志位（协议头第2字节的低4位）
     * @return true表示包含序列号，false表示不包含
     */
    static bool ContainsSequence(uint8_t message_flags);

    /**
     * @brief 检查flags中是否包含事件
     *
     * 检查消息标志位中是否设置了MSG_WITH_EVENT位。
     * 如果包含事件，需要在解析时读取4字节的事件编号字段。
     *
     * @param message_flags 消息标志位（协议头第2字节的低4位）
     * @return true表示包含事件，false表示不包含
     */
    static bool ContainsEvent(uint8_t message_flags);

    /**
     * @brief 检查事件是否需要跳过session_id
     *
     * 某些事件（如连接控制事件）不包含session_id字段。
     * 需要跳过session_id的事件：
     * - 1: START_CONNECTION
     * - 2: FINISH_CONNECTION
     * - 50: CONNECTION_STARTED
     * - 51: CONNECTION_FAILED
     * - 52: CONNECTION_FINISHED
     *
     * @param event 事件编号
     * @return true表示应跳过session_id，false表示需要读取session_id
     */
    static bool ShouldSkipSessionID(uint32_t event);

    /**
     * @brief 检查事件是否需要读取connect_id
     *
     * 某些连接相关事件包含connect_id字段（服务器分配的连接标识）。
     * 需要读取connect_id的事件：
     * - 50: CONNECTION_STARTED
     * - 51: CONNECTION_FAILED
     * - 52: CONNECTION_FINISHED
     *
     * @param event 事件编号
     * @return true表示需要读取connect_id，false表示不需要
     */
    static bool ShouldReadConnectID(uint32_t event);
};

} // namespace common 
} // namespace interview
