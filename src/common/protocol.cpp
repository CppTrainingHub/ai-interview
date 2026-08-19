#include "common/protocol.h"
#include <zlib.h>
#include <stdexcept>
#include <cstring>
#include <bitset>
#include "common/logger.h"

namespace interview {
namespace common {

// uint8_t转换为4位二进制字符串格式
inline std::string ToBinary4(uint8_t value) {
    return "0b" + std::bitset<4>(value).to_string();
}

// uint8_t转换为完整8位二进制字符串
inline std::string ToBinary8(uint8_t value) {
    return "0b" + std::bitset<8>(value).to_string();
}

ProtocolHeader::ProtocolHeader()
    : version(PROTOCOL_VERSION)
    , header_size(DEFAULT_HEADER_SIZE)
    , message_type(MessageType::CLIENT_FULL_REQUEST)
    , flags(MessageFlags::MSG_WITH_EVENT)
    , serialization(SerializationMethod::JSON)
    , compression(CompressionType::GZIP)
    , reserved(0) {
}

std::vector<uint8_t> Protocol::GenerateHeader(
    MessageType message_type,
    MessageFlags flags,
    SerializationMethod serialization,
    CompressionType compression,
    uint8_t reserved) {

    std::vector<uint8_t> header;
    header.reserve(4);

    // 第1字节: version(4bits) + header_size(4bits)
    header.push_back((PROTOCOL_VERSION << 4) | DEFAULT_HEADER_SIZE);

    // 第2字节: message_type(4bits) + flags(4bits)
    header.push_back((static_cast<uint8_t>(message_type) << 4) | static_cast<uint8_t>(flags));

    // 第3字节: serialization(4bits) + compression(4bits)
    header.push_back((static_cast<uint8_t>(serialization) << 4) | static_cast<uint8_t>(compression));

    // 第4字节: reserved
    header.push_back(reserved);

    return header;
}

ParsedResponse Protocol::ParseResponse(const std::vector<uint8_t>& data) {
    ParsedResponse result;

    if (data.size() < 4) {
        LOG_ERROR("ParseResponse: Data too short (", data.size(), " bytes), need at least 4 bytes for header");
        return result;
    }

    // 解析头部
    uint8_t version = data[0] >> 4;
    uint8_t header_size = data[0] & 0x0F;
    uint8_t message_type = data[1] >> 4;
    uint8_t message_flags = data[1] & 0x0F;
    uint8_t serialization = data[2] >> 4;
    uint8_t compression = data[2] & 0x0F;

    size_t payload_start = header_size * 4;
    if (data.size() < payload_start) {
        LOG_ERROR("ParseResponse: Data size (", data.size(), ") < header size (", payload_start, ")");
        return result;
    }

    const uint8_t* payload_ptr = data.data() + payload_start;
    size_t remaining = data.size() - payload_start;
    size_t offset = 0;

    // 判断消息类型
    if (message_type == static_cast<uint8_t>(MessageType::SERVER_FULL_RESPONSE) ||
        message_type == static_cast<uint8_t>(MessageType::SERVER_ACK)) {

        result.message_type = (message_type == static_cast<uint8_t>(MessageType::SERVER_ACK))
            ? "SERVER_ACK" : "SERVER_FULL_RESPONSE";

        // 1. 解析序列号（如果flags中包含）
        if (ContainsSequence(message_flags)) {
            if (remaining < offset + 4) {
                LOG_ERROR("Not enough bytes for sequence at offset ", offset);
                return result;
            }
            result.sequence = ReadInt32BigEndian(payload_ptr + offset);
            offset += 4;
        }

        // 2. 解析事件（如果flags中包含）
        if (ContainsEvent(message_flags)) {
            if (remaining < offset + 4) {
                LOG_ERROR("Not enough bytes for event at offset ", offset);
                return result;
            }
            result.event = ReadUint32BigEndian(payload_ptr + offset);
            offset += 4;

            // 3. 解析session_id（根据event判断是否跳过）
            if (!ShouldSkipSessionID(result.event)) {
                if (remaining < offset + 4) {
                    LOG_ERROR("Not enough bytes for session_id length at offset ", offset);
                    return result;
                }
                uint32_t session_id_size = ReadUint32BigEndian(payload_ptr + offset);
                offset += 4;

                if (remaining < offset + session_id_size) {
                    LOG_ERROR("Not enough bytes for session_id data at offset ", offset);
                    return result;
                }
                result.session_id = std::string(
                    reinterpret_cast<const char*>(payload_ptr + offset),
                    session_id_size
                );
                offset += session_id_size;
            }

            // 4. 解析connect_id（如果是连接相关事件）
            if (ShouldReadConnectID(result.event)) {
                if (remaining < offset + 4) {
                    LOG_ERROR("Not enough bytes for connect_id length at offset ", offset);
                    return result;
                }
                uint32_t connect_id_size = ReadUint32BigEndian(payload_ptr + offset);
                offset += 4;

                if (remaining < offset + connect_id_size) {
                    LOG_ERROR("Not enough bytes for connect_id data at offset ", offset);
                    return result;
                }
                result.connect_id = std::string(
                    reinterpret_cast<const char*>(payload_ptr + offset),
                    connect_id_size
                );
                offset += connect_id_size;
            }
        }

        // 5. 解析payload
        if (remaining < offset + 4) {
            LOG_ERROR("Not enough bytes for payload size at offset ", offset);
            return result;
        }
        result.payload_size = ReadUint32BigEndian(payload_ptr + offset);
        offset += 4;

        if (remaining < offset + result.payload_size) {
            LOG_ERROR("Not enough bytes for payload data at offset ", offset,
                      " (need ", result.payload_size, " bytes, have ", remaining - offset, ")");
            return result;
        }

        std::vector<uint8_t> payload_data(
            payload_ptr + offset,
            payload_ptr + offset + result.payload_size
        );

        // 解压
        if (compression == static_cast<uint8_t>(CompressionType::GZIP)) {
            size_t compressed_size = payload_data.size();
            try {
                payload_data = DecompressGzip(payload_data);
            } catch (const std::exception& e) {
                LOG_ERROR("GZIP decompression failed: ", e.what());
                return result;
            }
        } 

        // 反序列化
        if (serialization == static_cast<uint8_t>(SerializationMethod::JSON)) {
            try {
                std::string json_str(payload_data.begin(), payload_data.end());
                result.payload = nlohmann::json::parse(json_str);
                result.is_binary = false;
            } catch (const std::exception& e) {
                LOG_WARNING("JSON parsing failed: ", e.what(), " - treating as binary data");
                result.is_binary = true;
                result.payload_bytes = payload_data;
            }
        } else if (serialization == static_cast<uint8_t>(SerializationMethod::NO_SERIALIZATION)) {
            result.is_binary = true;
            result.payload_bytes = payload_data;
        } else {
            LOG_WARNING("Unknown serialization method: ", ToBinary4(serialization));
        }

    } else if (message_type == static_cast<uint8_t>(MessageType::SERVER_ERROR_RESPONSE)) {
        result.message_type = "SERVER_ERROR";

        // 错误响应: error_code + payload_size + payload
        if (remaining < 4) {
            LOG_ERROR("Not enough bytes for error code");
            return result;
        }
        result.code = ReadUint32BigEndian(payload_ptr);
        offset += 4;

        if (remaining < offset + 4) {
            LOG_ERROR("Not enough bytes for error payload size");
            return result;
        }
        result.payload_size = ReadUint32BigEndian(payload_ptr + offset);
        offset += 4;

        if (remaining < offset + result.payload_size) {
            LOG_ERROR("Not enough bytes for error payload data");
            return result;
        }
        std::vector<uint8_t> payload_data(
            payload_ptr + offset,
            payload_ptr + offset + result.payload_size
        );

        if (compression == static_cast<uint8_t>(CompressionType::GZIP)) {
            size_t compressed_size = payload_data.size();
            try {
                payload_data = DecompressGzip(payload_data);
            } catch (const std::exception& e) {
                LOG_ERROR("GZIP decompression failed: ", e.what());
                return result;
            }
        }

        std::string error_msg(payload_data.begin(), payload_data.end());
        result.payload = nlohmann::json{{"error", error_msg}};

    } else {
        LOG_ERROR("Unknown message type: ", ToBinary4(message_type), " (", static_cast<int>(message_type), ")");
    }

    return result;
}

std::vector<uint8_t> Protocol::CompressGzip(const std::vector<uint8_t>& data) {
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    // 使用gzip格式 (windowBits = 15 + 16)
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        throw std::runtime_error("Failed to initialize gzip compression");
    }

    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_in = const_cast<uint8_t*>(data.data());

    std::vector<uint8_t> compressed;
    compressed.resize(deflateBound(&stream, static_cast<uLong>(data.size())));

    stream.avail_out = static_cast<uInt>(compressed.size());
    stream.next_out = compressed.data();

    if (deflate(&stream, Z_FINISH) != Z_STREAM_END) {
        deflateEnd(&stream);
        throw std::runtime_error("Failed to compress data");
    }

    compressed.resize(stream.total_out);
    deflateEnd(&stream);

    return compressed;
}

std::vector<uint8_t> Protocol::DecompressGzip(const std::vector<uint8_t>& data) {
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    // 使用gzip格式 (windowBits = 15 + 16)
    if (inflateInit2(&stream, 15 + 16) != Z_OK) {
        throw std::runtime_error("Failed to initialize gzip decompression");
    }

    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_in = const_cast<uint8_t*>(data.data());

    std::vector<uint8_t> decompressed;
    constexpr size_t CHUNK_SIZE = 32768;

    int ret;
    do {
        size_t old_size = decompressed.size();
        decompressed.resize(old_size + CHUNK_SIZE);

        stream.avail_out = static_cast<uInt>(CHUNK_SIZE);
        stream.next_out = decompressed.data() + old_size;

        ret = inflate(&stream, Z_NO_FLUSH);

        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&stream);
            throw std::runtime_error("Failed to decompress data");
        }

        decompressed.resize(old_size + CHUNK_SIZE - stream.avail_out);
    } while (ret != Z_STREAM_END);

    inflateEnd(&stream);
    return decompressed;
}

void Protocol::AppendUint32BigEndian(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back((value >> 24) & 0xFF);
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

uint32_t Protocol::ReadUint32BigEndian(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

int32_t Protocol::ReadInt32BigEndian(const uint8_t* data) {
    uint32_t unsigned_value = ReadUint32BigEndian(data);
    return static_cast<int32_t>(unsigned_value);
}

bool Protocol::ContainsSequence(uint8_t message_flags) {
    // 检查flags中是否设置了POS_SEQUENCE或NEG_SEQUENCE位
    constexpr uint8_t POS_SEQ = static_cast<uint8_t>(MessageFlags::POS_SEQUENCE);
    constexpr uint8_t NEG_SEQ = static_cast<uint8_t>(MessageFlags::NEG_SEQUENCE);
    return (message_flags & POS_SEQ) == POS_SEQ || (message_flags & NEG_SEQ) == NEG_SEQ;
}

bool Protocol::ContainsEvent(uint8_t message_flags) {
    // 检查flags中是否设置了MSG_WITH_EVENT位
    constexpr uint8_t EVENT_FLAG = static_cast<uint8_t>(MessageFlags::MSG_WITH_EVENT);
    return (message_flags & EVENT_FLAG) == EVENT_FLAG;
}

bool Protocol::ShouldSkipSessionID(uint32_t event) {
    // 事件1,2,50,51,52不包含session_id
    // 1=StartConnection, 2=FinishConnection,
    // 50=ConnectionStarted, 51=ConnectionFailed, 52=ConnectionFinished
    return event == 1 || event == 2 || event == 50 || event == 51 || event == 52;
}

bool Protocol::ShouldReadConnectID(uint32_t event) {
    // 事件50,51,52包含connect_id
    // 50=ConnectionStarted, 51=ConnectionFailed, 52=ConnectionFinished
    return event == 50 || event == 51 || event == 52;
}

std::vector<uint8_t> Protocol::BuildFullRequest(
    uint32_t event,
    const std::string& session_id,
    const nlohmann::json& payload) {

    std::vector<uint8_t> message;

    // 1. 添加协议头
    auto header = GenerateHeader(
        MessageType::CLIENT_FULL_REQUEST,
        MessageFlags::MSG_WITH_EVENT,
        SerializationMethod::JSON,
        CompressionType::NO_COMPRESSION
    );
    message.insert(message.end(), header.begin(), header.end());
    LOG_INFO("GenerateHeader: ", header.size(), " bytes");

    // 2. 添加event (4字节大端)
    AppendUint32BigEndian(message, event);

    // 3. 添加session_id长度和内容（根据event类型判断是否需要）
    if (!ShouldSkipSessionID(event)) {
        AppendUint32BigEndian(message, static_cast<uint32_t>(session_id.size()));
        message.insert(message.end(), session_id.begin(), session_id.end());
    }

    // 4. 序列化payload（不压缩）
    std::string payload_str = payload.dump();
    std::vector<uint8_t> payload_bytes(payload_str.begin(), payload_str.end());

    // 5. 添加payload大小和内容
    AppendUint32BigEndian(message, static_cast<uint32_t>(payload_bytes.size()));
    message.insert(message.end(), payload_bytes.begin(), payload_bytes.end());

    return message;
}

std::vector<uint8_t> Protocol::BuildClientAudioRequest(
    uint32_t event,
    const std::string& session_id,
    const std::vector<uint8_t>& audio_data) {

    std::vector<uint8_t> message;

    // 1. 添加协议头（音频请求不使用JSON序列化）
    auto header = GenerateHeader(
        MessageType::CLIENT_AUDIO_ONLY_REQUEST,
        MessageFlags::MSG_WITH_EVENT,
        SerializationMethod::NO_SERIALIZATION,
        CompressionType::NO_COMPRESSION
    );
    message.insert(message.end(), header.begin(), header.end());

    // 2. 添加event (4字节大端)
    AppendUint32BigEndian(message, event);

    // 3. 添加session_id长度和内容（根据event类型判断是否需要）
    if (!ShouldSkipSessionID(event)) {
        AppendUint32BigEndian(message, static_cast<uint32_t>(session_id.size()));
        message.insert(message.end(), session_id.begin(), session_id.end());
    }

    // 4. 添加音频数据（不压缩）
    // 5. 添加payload大小和内容
    AppendUint32BigEndian(message, static_cast<uint32_t>(audio_data.size()));
    message.insert(message.end(), audio_data.begin(), audio_data.end());

    return message;
}

} // namespace common 
} // namespace interview
