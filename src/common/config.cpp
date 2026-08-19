#include "common/config.h"
#include "common/utils.h"
#include <fstream>
#include <stdexcept>

namespace interview {
namespace common {

WebSocketConfig::WebSocketConfig() = default;

DialogConfig::DialogConfig() = default;

LLMConfig::LLMConfig() = default;

Config& Config::Instance() {
    static Config instance;
    return instance;
}

Config::Config() = default;

namespace {

template <typename T>
T Require(const nlohmann::json& obj, const char* key) {
    if (!obj.contains(key)) {
        throw std::runtime_error(std::string("Missing required configuration key: ") + key);
    }
    return obj.at(key).get<T>();
}

const nlohmann::json& RequireObject(const nlohmann::json& obj, const char* key) {
    if (!obj.contains(key)) {
        throw std::runtime_error(std::string("Missing required configuration section: ") + key);
    }
    const auto& value = obj.at(key);
    if (!value.is_object()) {
        throw std::runtime_error(std::string("Configuration section must be object: ") + key);
    }
    return value;
}

} // namespace

void Config::LoadFromFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open configuration file: " + path);
    }

    nlohmann::json root;
    try {
        ifs >> root;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to parse configuration file: ") + e.what());
    }

    const auto& audio = RequireObject(root, "audio");
    const auto& input = RequireObject(audio, "input");
    input_audio_config.chunk = Require<int>(input, "chunk");
    input_audio_config.channels = Require<int>(input, "channels");
    input_audio_config.sample_rate = Require<int>(input, "sample_rate");
    input_audio_config.bit_size = Require<int>(input, "bit_size");
    input_audio_config.format = Require<std::string>(input, "format");

    const auto& output = RequireObject(audio, "output");
    output_audio_config.chunk = Require<int>(output, "chunk");
    output_audio_config.channels = Require<int>(output, "channels");
    output_audio_config.sample_rate = Require<int>(output, "sample_rate");
    output_audio_config.bit_size = Require<int>(output, "bit_size");
    output_audio_config.format = Require<std::string>(output, "format");

    const auto& ws = RequireObject(root, "ws");
    ws_config.base_url = Require<std::string>(ws, "base_url");
    ws_config.headers.clear();
    const auto& headers_obj = RequireObject(ws, "headers");
    for (const auto& item : headers_obj.items()) {
        ws_config.headers[item.key()] = item.value().get<std::string>();
    }
    auto connect_id_it = ws_config.headers.find("X-Api-Connect-Id");
    if (connect_id_it == ws_config.headers.end() || connect_id_it->second.empty()) {
        ws_config.headers["X-Api-Connect-Id"] = common::GenerateUUID();
    }

    const auto& dialog = RequireObject(root, "dialog");
    dialog_config.bot_name = Require<std::string>(dialog, "bot_name");
    dialog_config.system_role = Require<std::string>(dialog, "system_role");
    dialog_config.speaking_style = Require<std::string>(dialog, "speaking_style");
    dialog_config.city = Require<std::string>(dialog, "city");
    dialog_config.strict_audit = dialog.value("strict_audit", false);
    dialog_config.audit_response = Require<std::string>(dialog, "audit_response");
    dialog_config.recv_timeout = Require<int>(dialog, "recv_timeout");
    dialog_config.input_mod = Require<std::string>(dialog, "input_mod");

    const auto& tts = RequireObject(root, "tts");
    tts_config.speaker = Require<std::string>(tts, "speaker");
    tts_config.channel = Require<int>(tts, "channel");
    tts_config.format = Require<std::string>(tts, "format");
    tts_config.sample_rate = Require<int>(tts, "sample_rate");

    const auto& asr = RequireObject(root, "asr");
    asr_config.end_smooth_window_ms = Require<int>(asr, "end_smooth_window_ms");
    asr_config.vad_silence_duration = Require<int>(asr, "vad_silence_duration");
    asr_config.vad_speech_trigger_duration = Require<int>(asr, "vad_speech_trigger_duration");

    const auto& llm = RequireObject(root, "llm");
    llm_config.api_url = Require<std::string>(llm, "api_url");
    llm_config.api_key = Require<std::string>(llm, "api_key");
    llm_config.model = Require<std::string>(llm, "model");
    llm_config.temperature = static_cast<float>(Require<double>(llm, "temperature"));
    llm_config.max_tokens = Require<int>(llm, "max_tokens");
    llm_config.timeout_seconds = Require<int>(llm, "timeout_seconds");

    // 验证配置参数有效性
    ValidateConfiguration();
}

void Config::ValidateConfiguration() const {
    // 验证音频配置
    if (input_audio_config.sample_rate <= 0 || input_audio_config.sample_rate > 192000) {
        throw std::runtime_error("Invalid input sample rate: " + std::to_string(input_audio_config.sample_rate));
    }
    if (output_audio_config.sample_rate <= 0 || output_audio_config.sample_rate > 192000) {
        throw std::runtime_error("Invalid output sample rate: " + std::to_string(output_audio_config.sample_rate));
    }
    if (input_audio_config.channels <= 0 || input_audio_config.channels > 8) {
        throw std::runtime_error("Invalid input channels: " + std::to_string(input_audio_config.channels));
    }
    if (output_audio_config.channels <= 0 || output_audio_config.channels > 8) {
        throw std::runtime_error("Invalid output channels: " + std::to_string(output_audio_config.channels));
    }
    if (input_audio_config.chunk <= 0 || input_audio_config.chunk > 65536) {
        throw std::runtime_error("Invalid input chunk size: " + std::to_string(input_audio_config.chunk));
    }
    if (output_audio_config.chunk <= 0 || output_audio_config.chunk > 65536) {
        throw std::runtime_error("Invalid output chunk size: " + std::to_string(output_audio_config.chunk));
    }

    // 验证WebSocket配置
    if (ws_config.base_url.empty()) {
        throw std::runtime_error("WebSocket base_url cannot be empty");
    }
    if (ws_config.base_url.substr(0, 6) != "wss://" && ws_config.base_url.substr(0, 5) != "ws://") {
        throw std::runtime_error("Invalid WebSocket URL scheme (must start with ws:// or wss://)");
    }

    // 验证LLM配置
    if (llm_config.api_url.empty()) {
        throw std::runtime_error("LLM API URL cannot be empty");
    }
    if (llm_config.api_key.empty()) {
        throw std::runtime_error("LLM API key cannot be empty");
    }
    if (llm_config.model.empty()) {
        throw std::runtime_error("LLM model name cannot be empty");
    }
    if (llm_config.temperature < 0.0f || llm_config.temperature > 2.0f) {
        throw std::runtime_error("LLM temperature must be between 0.0 and 2.0");
    }
    if (llm_config.max_tokens <= 0 || llm_config.max_tokens > 100000) {
        throw std::runtime_error("LLM max_tokens must be between 1 and 100000");
    }
    if (llm_config.timeout_seconds <= 0 || llm_config.timeout_seconds > 600) {
        throw std::runtime_error("LLM timeout_seconds must be between 1 and 600");
    }

    // 验证VAD配置
    if (asr_config.vad_silence_duration < 100 || asr_config.vad_silence_duration > 10000) {
        throw std::runtime_error("VAD silence duration must be between 100ms and 10000ms");
    }
    if (asr_config.vad_speech_trigger_duration < 50 || asr_config.vad_speech_trigger_duration > 5000) {
        throw std::runtime_error("VAD speech trigger duration must be between 50ms and 5000ms");
    }
}

nlohmann::json Config::GenerateStartSessionRequest() const {
    nlohmann::json request;

    // ASR配置
    request["asr"]["extra"]["end_smooth_window_ms"] = asr_config.end_smooth_window_ms;
    request["asr"]["extra"]["vad_silence_duration"] = asr_config.vad_silence_duration;
    request["asr"]["extra"]["vad_speech_trigger_duration"] = asr_config.vad_speech_trigger_duration;

    // TTS配置
    request["tts"]["speaker"] = tts_config.speaker;
    request["tts"]["audio_config"]["channel"] = tts_config.channel;
    request["tts"]["audio_config"]["format"] = tts_config.format;
    request["tts"]["audio_config"]["sample_rate"] = tts_config.sample_rate;

    // Dialog配置
    request["dialog"]["bot_name"] = dialog_config.bot_name;
    request["dialog"]["system_role"] = dialog_config.system_role;
    request["dialog"]["speaking_style"] = dialog_config.speaking_style;
    request["dialog"]["location"]["city"] = dialog_config.city;
    request["dialog"]["extra"]["strict_audit"] = dialog_config.strict_audit;
    request["dialog"]["extra"]["audit_response"] = dialog_config.audit_response;
    request["dialog"]["extra"]["recv_timeout"] = dialog_config.recv_timeout;
    request["dialog"]["extra"]["input_mod"] = dialog_config.input_mod;

    return request;
}

} // namespace common 
} // namespace interview
