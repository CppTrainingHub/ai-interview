#include "audio/volcengine_realtime_speech_client.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

#include "audio/realtime_speech_protocol.h"

#if defined(AI_INTERVIEW_ENABLE_VOLCENGINE_REALTIME)
#include <curl/curl.h>
#endif

namespace interview {
namespace audio {
namespace {

void ValidatePcm16Format(const AudioFormat& format, const char* name) {
    if (format.sample_rate_hz <= 0) {
        throw std::runtime_error(std::string(name) + " sample_rate_hz must be positive");
    }
    if (format.channels <= 0) {
        throw std::runtime_error(std::string(name) + " channels must be positive");
    }
    if (format.sample_format != SampleFormat::kPcmS16Le) {
        throw std::runtime_error(std::string(name) + " must use 16-bit little-endian PCM");
    }
}

std::string GenerateHex(std::size_t count) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<int> distribution(0, 15);

    std::string value;
    value.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        value.push_back(kDigits[distribution(generator)]);
    }
    return value;
}

std::string GenerateUuidLikeId() {
    std::ostringstream output;
    output << GenerateHex(8) << '-'
           << GenerateHex(4) << '-'
           << GenerateHex(4) << '-'
           << GenerateHex(4) << '-'
           << GenerateHex(12);
    return output.str();
}

std::vector<std::vector<unsigned char>> SplitAudio(
    const std::vector<unsigned char>& audio,
    const AudioFormat& format) {
    const auto bytes_per_200ms =
        static_cast<std::size_t>(format.sample_rate_hz) *
        format.BytesPerFrame() / 5U;
    const auto chunk_size = std::max<std::size_t>(bytes_per_200ms, format.BytesPerFrame());

    std::vector<std::vector<unsigned char>> chunks;
    for (std::size_t offset = 0; offset < audio.size(); offset += chunk_size) {
        const auto end = std::min(audio.size(), offset + chunk_size);
        chunks.emplace_back(audio.begin() + static_cast<std::ptrdiff_t>(offset),
                            audio.begin() + static_cast<std::ptrdiff_t>(end));
    }
    return chunks;
}

} // namespace

nlohmann::json VolcengineRealtimeSpeechConfig::BuildStartSessionPayload() const {
    ValidatePcm16Format(input_format, "input_format");
    ValidatePcm16Format(output_format, "output_format");

    nlohmann::json request;
    request["asr"]["extra"]["end_smooth_window_ms"] = asr_end_smooth_window_ms;
    request["asr"]["extra"]["vad_silence_duration"] = asr_vad_silence_duration_ms;
    request["asr"]["extra"]["vad_speech_trigger_duration"] =
        asr_vad_speech_trigger_duration_ms;

    request["tts"]["speaker"] = speaker;
    request["tts"]["audio_config"]["channel"] = output_format.channels;
    request["tts"]["audio_config"]["format"] = audio_format_name;
    request["tts"]["audio_config"]["sample_rate"] = output_format.sample_rate_hz;

    request["dialog"]["bot_name"] = bot_name;
    request["dialog"]["system_role"] = system_role;
    request["dialog"]["speaking_style"] = speaking_style;
    request["dialog"]["location"]["city"] = city;
    request["dialog"]["extra"]["strict_audit"] = strict_audit;
    request["dialog"]["extra"]["audit_response"] = audit_response;
    request["dialog"]["extra"]["recv_timeout"] = recv_timeout_seconds;
    request["dialog"]["extra"]["input_mod"] = input_mode;
    return request;
}

class VolcengineRealtimeSpeechClient::Impl {
public:
    explicit Impl(VolcengineRealtimeSpeechConfig config)
        : config_(std::move(config)),
          session_id_(GenerateUuidLikeId()) {
        ValidatePcm16Format(config_.input_format, "input_format");
        ValidatePcm16Format(config_.output_format, "output_format");
    }

    ~Impl() {
        Close();
    }

    void Connect() {
#if defined(AI_INTERVIEW_ENABLE_VOLCENGINE_REALTIME) && LIBCURL_VERSION_NUM >= 0x075600
        if (connected_) {
            return;
        }

        curl_ = curl_easy_init();
        if (curl_ == nullptr) {
            throw std::runtime_error("failed to initialize libcurl");
        }

        headers_ = nullptr;
        for (const auto& [key, value] : config_.headers) {
            const std::string header = key + ": " + value;
            headers_ = curl_slist_append(headers_, header.c_str());
        }

        curl_easy_setopt(curl_, CURLOPT_URL, config_.websocket_url.c_str());
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers_);
        curl_easy_setopt(curl_, CURLOPT_CONNECT_ONLY, 2L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, static_cast<long>(config_.operation_timeout_ms));

        const CURLcode result = curl_easy_perform(curl_);
        if (result != CURLE_OK) {
            const std::string message = curl_easy_strerror(result);
            CleanupCurl();
            throw std::runtime_error("failed to connect realtime speech websocket: " + message);
        }

        connected_ = true;
        SendMessage(BuildRealtimeFullRequest(
            realtime_events::kStartConnection,
            "",
            nlohmann::json::object()
        ));
        (void)ReceiveMessage();

        SendMessage(BuildRealtimeFullRequest(
            realtime_events::kStartSession,
            session_id_,
            config_.BuildStartSessionPayload()
        ));
#else
        throw std::runtime_error(
            "Volcengine realtime speech client requires AI_INTERVIEW_ENABLE_VOLCENGINE_REALTIME "
            "and libcurl 7.86.0 or newer"
        );
#endif
    }

    void Close() {
#if defined(AI_INTERVIEW_ENABLE_VOLCENGINE_REALTIME) && LIBCURL_VERSION_NUM >= 0x075600
        if (!connected_) {
            CleanupCurl();
            return;
        }

        try {
            SendMessage(BuildRealtimeFullRequest(
                realtime_events::kFinishSession,
                session_id_,
                nlohmann::json::object()
            ));
            SendMessage(BuildRealtimeFullRequest(
                realtime_events::kFinishConnection,
                "",
                nlohmann::json::object()
            ));
        } catch (...) {
        }

        connected_ = false;
        CleanupCurl();
#endif
    }

    std::string Transcribe(const PcmAudio& pcm_audio) {
        if (pcm_audio.bytes.empty()) {
            return "";
        }
        if (pcm_audio.format.sample_rate_hz != config_.input_format.sample_rate_hz ||
            pcm_audio.format.channels != config_.input_format.channels ||
            pcm_audio.format.sample_format != config_.input_format.sample_format) {
            throw std::runtime_error("ASR input format does not match Volcengine configuration");
        }
        EnsureConnected();

        const std::vector<unsigned char> bytes(pcm_audio.bytes.begin(), pcm_audio.bytes.end());
        for (const auto& chunk : SplitAudio(bytes, config_.input_format)) {
            SendMessage(BuildRealtimeAudioRequest(
                realtime_events::kTaskRequest,
                session_id_,
                chunk
            ));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        const auto deadline = Deadline();
        while (std::chrono::steady_clock::now() < deadline) {
            const auto response = ParseRealtimeSpeechResponse(ReceiveMessage());
            ThrowIfServerError(response);
            if (response.event == realtime_events::kAsrResult) {
                const auto text = ExtractFinalAsrText(response.payload);
                if (!text.empty()) {
                    return text;
                }
            }
        }

        return "";
    }

    PcmAudio Synthesize(const std::string& text) {
        PcmAudio result;
        result.format = config_.output_format;
        if (text.empty()) {
            return result;
        }
        EnsureConnected();

        nlohmann::json payload;
        payload["content"] =
            "直接朗读下面文字：" + text +
            "后续对应于面试者的回答都只回复[好的，我们继续],不要承接或评论";

        SendMessage(BuildRealtimeFullRequest(
            realtime_events::kChatTextQuery,
            session_id_,
            payload
        ));

        std::vector<unsigned char> audio;
        const auto deadline = Deadline();
        while (std::chrono::steady_clock::now() < deadline) {
            const auto response = ParseRealtimeSpeechResponse(ReceiveMessage());
            ThrowIfServerError(response);
            if (response.is_binary && !response.payload_bytes.empty()) {
                audio.insert(
                    audio.end(),
                    response.payload_bytes.begin(),
                    response.payload_bytes.end()
                );
            }
            if (response.event == realtime_events::kTtsEnd) {
                break;
            }
        }

        result.bytes.assign(audio.begin(), audio.end());
        return result;
    }

private:
    void EnsureConnected() {
        if (!connected_) {
            Connect();
        }
    }

    std::chrono::steady_clock::time_point Deadline() const {
        return std::chrono::steady_clock::now() +
               std::chrono::milliseconds(config_.operation_timeout_ms);
    }

    static void ThrowIfServerError(const RealtimeSpeechResponse& response) {
        if (response.message_type != "SERVER_ERROR") {
            return;
        }

        throw std::runtime_error("realtime speech server error: " + response.payload.dump());
    }

    void SendMessage(const std::vector<unsigned char>& message) {
#if defined(AI_INTERVIEW_ENABLE_VOLCENGINE_REALTIME) && LIBCURL_VERSION_NUM >= 0x075600
        std::size_t sent = 0;
        const CURLcode result = curl_ws_send(
            curl_,
            message.data(),
            message.size(),
            &sent,
            0,
            CURLWS_BINARY
        );
        if (result != CURLE_OK || sent != message.size()) {
            throw std::runtime_error(
                std::string("failed to send realtime websocket message: ") +
                curl_easy_strerror(result)
            );
        }
#else
        (void)message;
        throw std::runtime_error("realtime websocket send is unavailable");
#endif
    }

    std::vector<unsigned char> ReceiveMessage() {
#if defined(AI_INTERVIEW_ENABLE_VOLCENGINE_REALTIME) && LIBCURL_VERSION_NUM >= 0x075600
        std::vector<unsigned char> message;
        std::vector<unsigned char> buffer(16384);

        while (true) {
            std::size_t received = 0;
            const struct curl_ws_frame* frame = nullptr;
            const CURLcode result = curl_ws_recv(
                curl_,
                buffer.data(),
                buffer.size(),
                &received,
                &frame
            );
            if (result != CURLE_OK) {
                throw std::runtime_error(
                    std::string("failed to receive realtime websocket message: ") +
                    curl_easy_strerror(result)
                );
            }
            message.insert(message.end(), buffer.begin(), buffer.begin() + received);
            if (frame == nullptr || frame->bytesleft == 0) {
                return message;
            }
        }
#else
        throw std::runtime_error("realtime websocket receive is unavailable");
#endif
    }

    void CleanupCurl() {
#if defined(AI_INTERVIEW_ENABLE_VOLCENGINE_REALTIME) && LIBCURL_VERSION_NUM >= 0x075600
        if (headers_ != nullptr) {
            curl_slist_free_all(headers_);
            headers_ = nullptr;
        }
        if (curl_ != nullptr) {
            curl_easy_cleanup(curl_);
            curl_ = nullptr;
        }
#endif
    }

    VolcengineRealtimeSpeechConfig config_;
    std::string session_id_;
    bool connected_ = false;

#if defined(AI_INTERVIEW_ENABLE_VOLCENGINE_REALTIME) && LIBCURL_VERSION_NUM >= 0x075600
    CURL* curl_ = nullptr;
    curl_slist* headers_ = nullptr;
#endif
};

VolcengineRealtimeSpeechClient::VolcengineRealtimeSpeechClient(
    VolcengineRealtimeSpeechConfig config)
    : pImpl(std::make_unique<Impl>(std::move(config))) {}

VolcengineRealtimeSpeechClient::~VolcengineRealtimeSpeechClient() = default;

VolcengineRealtimeSpeechClient::VolcengineRealtimeSpeechClient(
    VolcengineRealtimeSpeechClient&&) noexcept = default;

VolcengineRealtimeSpeechClient& VolcengineRealtimeSpeechClient::operator=(
    VolcengineRealtimeSpeechClient&&) noexcept = default;

void VolcengineRealtimeSpeechClient::Connect() {
    pImpl->Connect();
}

void VolcengineRealtimeSpeechClient::Close() {
    pImpl->Close();
}

std::string VolcengineRealtimeSpeechClient::Transcribe(
    const PcmAudio& pcm_audio) {
    return pImpl->Transcribe(pcm_audio);
}

PcmAudio VolcengineRealtimeSpeechClient::Synthesize(const std::string& text) {
    return pImpl->Synthesize(text);
}

} // namespace audio
} // namespace interview
