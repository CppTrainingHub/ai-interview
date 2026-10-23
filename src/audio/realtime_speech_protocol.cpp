#include "audio/realtime_speech_protocol.h"

#include <stdexcept>

namespace interview {
namespace audio {
namespace {

constexpr unsigned char kProtocolVersion = 0b0001;
constexpr unsigned char kDefaultHeaderSize = 0b0001;

enum class MessageType : unsigned char {
    kClientFullRequest = 0b0001,
    kClientAudioOnlyRequest = 0b0010,
    kServerFullResponse = 0b1001,
    kServerAck = 0b1011,
    kServerErrorResponse = 0b1111
};

enum class MessageFlags : unsigned char {
    kNoSequence = 0b0000,
    kPosSequence = 0b0001,
    kNegSequence = 0b0010,
    kWithEvent = 0b0100
};

enum class SerializationMethod : unsigned char {
    kNone = 0b0000,
    kJson = 0b0001
};

enum class CompressionType : unsigned char {
    kNone = 0b0000,
    kGzip = 0b0001
};

std::vector<unsigned char> GenerateHeader(MessageType message_type,
                                          MessageFlags flags,
                                          SerializationMethod serialization,
                                          CompressionType compression) {
    return {
        static_cast<unsigned char>((kProtocolVersion << 4) | kDefaultHeaderSize),
        static_cast<unsigned char>(
            (static_cast<unsigned char>(message_type) << 4) |
            static_cast<unsigned char>(flags)
        ),
        static_cast<unsigned char>(
            (static_cast<unsigned char>(serialization) << 4) |
            static_cast<unsigned char>(compression)
        ),
        0x00
    };
}

void AppendUint32BigEndian(std::vector<unsigned char>& buffer, std::uint32_t value) {
    buffer.push_back(static_cast<unsigned char>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<unsigned char>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<unsigned char>(value & 0xFF));
}

std::uint32_t ReadUint32BigEndian(const unsigned char* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

bool ContainsSequence(unsigned char flags) {
    return (flags & static_cast<unsigned char>(MessageFlags::kPosSequence)) != 0U ||
           (flags & static_cast<unsigned char>(MessageFlags::kNegSequence)) != 0U;
}

bool ContainsEvent(unsigned char flags) {
    return (flags & static_cast<unsigned char>(MessageFlags::kWithEvent)) != 0U;
}

bool ShouldSkipSessionId(std::uint32_t event) {
    return event == realtime_events::kStartConnection ||
           event == realtime_events::kFinishConnection ||
           event == realtime_events::kConnectionStarted ||
           event == realtime_events::kConnectionFailed ||
           event == realtime_events::kConnectionFinished;
}

bool ShouldReadConnectId(std::uint32_t event) {
    return event == realtime_events::kConnectionStarted ||
           event == realtime_events::kConnectionFailed ||
           event == realtime_events::kConnectionFinished;
}

std::string ReadSizedString(const unsigned char* data,
                            std::size_t remaining,
                            std::size_t& offset,
                            const char* field_name) {
    if (remaining < offset + 4U) {
        throw std::runtime_error(std::string("missing ") + field_name + " size");
    }

    const auto size = ReadUint32BigEndian(data + offset);
    offset += 4U;
    if (remaining < offset + size) {
        throw std::runtime_error(std::string("truncated ") + field_name);
    }

    std::string value(reinterpret_cast<const char*>(data + offset), size);
    offset += size;
    return value;
}

std::vector<unsigned char> BuildRequest(MessageType message_type,
                                        SerializationMethod serialization,
                                        std::uint32_t event,
                                        const std::string& session_id,
                                        const std::vector<unsigned char>& payload) {
    std::vector<unsigned char> message = GenerateHeader(
        message_type,
        MessageFlags::kWithEvent,
        serialization,
        CompressionType::kNone
    );

    AppendUint32BigEndian(message, event);
    if (!ShouldSkipSessionId(event)) {
        AppendUint32BigEndian(message, static_cast<std::uint32_t>(session_id.size()));
        message.insert(message.end(), session_id.begin(), session_id.end());
    }

    AppendUint32BigEndian(message, static_cast<std::uint32_t>(payload.size()));
    message.insert(message.end(), payload.begin(), payload.end());
    return message;
}

} // namespace

std::vector<unsigned char> BuildRealtimeFullRequest(std::uint32_t event,
                                                    const std::string& session_id,
                                                    const nlohmann::json& payload) {
    const std::string payload_text = payload.dump();
    const std::vector<unsigned char> payload_bytes(payload_text.begin(), payload_text.end());
    return BuildRequest(
        MessageType::kClientFullRequest,
        SerializationMethod::kJson,
        event,
        session_id,
        payload_bytes
    );
}

std::vector<unsigned char> BuildRealtimeAudioRequest(
    std::uint32_t event,
    const std::string& session_id,
    const std::vector<unsigned char>& audio_data) {
    return BuildRequest(
        MessageType::kClientAudioOnlyRequest,
        SerializationMethod::kNone,
        event,
        session_id,
        audio_data
    );
}

RealtimeSpeechResponse ParseRealtimeSpeechResponse(const std::vector<unsigned char>& data) {
    if (data.size() < 4U) {
        throw std::runtime_error("realtime response is shorter than protocol header");
    }

    const auto header_size = static_cast<std::size_t>(data[0] & 0x0F) * 4U;
    if (data.size() < header_size) {
        throw std::runtime_error("realtime response is shorter than declared header");
    }

    const auto message_type = static_cast<unsigned char>(data[1] >> 4);
    const auto message_flags = static_cast<unsigned char>(data[1] & 0x0F);
    const auto serialization = static_cast<unsigned char>(data[2] >> 4);
    const auto compression = static_cast<unsigned char>(data[2] & 0x0F);

    if (compression == static_cast<unsigned char>(CompressionType::kGzip)) {
        throw std::runtime_error(
            "gzip-compressed realtime responses are not supported by this lightweight parser"
        );
    }

    const unsigned char* payload = data.data() + header_size;
    const std::size_t remaining = data.size() - header_size;
    std::size_t offset = 0;

    RealtimeSpeechResponse response;
    if (message_type == static_cast<unsigned char>(MessageType::kServerFullResponse) ||
        message_type == static_cast<unsigned char>(MessageType::kServerAck)) {
        response.message_type =
            message_type == static_cast<unsigned char>(MessageType::kServerAck)
                ? "SERVER_ACK"
                : "SERVER_FULL_RESPONSE";

        if (ContainsSequence(message_flags)) {
            if (remaining < offset + 4U) {
                throw std::runtime_error("missing realtime response sequence");
            }
            offset += 4U;
        }

        if (ContainsEvent(message_flags)) {
            if (remaining < offset + 4U) {
                throw std::runtime_error("missing realtime response event");
            }
            response.event = ReadUint32BigEndian(payload + offset);
            offset += 4U;

            if (!ShouldSkipSessionId(response.event)) {
                response.session_id = ReadSizedString(
                    payload,
                    remaining,
                    offset,
                    "session_id"
                );
            }

            if (ShouldReadConnectId(response.event)) {
                response.connect_id = ReadSizedString(
                    payload,
                    remaining,
                    offset,
                    "connect_id"
                );
            }
        }

        if (remaining < offset + 4U) {
            throw std::runtime_error("missing realtime response payload size");
        }
        response.payload_size = ReadUint32BigEndian(payload + offset);
        offset += 4U;

        if (remaining < offset + response.payload_size) {
            throw std::runtime_error("truncated realtime response payload");
        }

        std::vector<unsigned char> payload_data(
            payload + offset,
            payload + offset + response.payload_size
        );

        if (serialization == static_cast<unsigned char>(SerializationMethod::kJson)) {
            const std::string json_text(payload_data.begin(), payload_data.end());
            response.payload = nlohmann::json::parse(json_text);
            response.is_binary = false;
        } else {
            response.payload_bytes = std::move(payload_data);
            response.is_binary = true;
        }

        return response;
    }

    if (message_type == static_cast<unsigned char>(MessageType::kServerErrorResponse)) {
        response.message_type = "SERVER_ERROR";
        if (remaining < 8U) {
            throw std::runtime_error("truncated realtime error response");
        }
        response.error_code = ReadUint32BigEndian(payload);
        response.payload_size = ReadUint32BigEndian(payload + 4U);
        if (remaining < 8U + response.payload_size) {
            throw std::runtime_error("truncated realtime error payload");
        }
        const std::string error_text(
            reinterpret_cast<const char*>(payload + 8U),
            response.payload_size
        );
        response.payload = nlohmann::json{{"error", error_text}};
        return response;
    }

    throw std::runtime_error("unknown realtime response message type");
}

std::string ExtractFinalAsrText(const nlohmann::json& payload) {
    const auto results = payload.value("results", nlohmann::json::array());
    if (results.empty() || !results.front().is_object()) {
        return "";
    }

    const auto& result = results.front();
    const bool is_interim = result.value("is_interim", true);
    if (is_interim) {
        return "";
    }

    return result.value("text", std::string());
}

} // namespace audio
} // namespace interview
