#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

#include "config/app_config.h"

namespace interview {
namespace config {
namespace {

constexpr const char* kDefaultLocalConfigPath = "config/app.local.env";

/**
 * @brief 读取环境变量，如果不存在或为空则返回默认值
 * @param name 环境变量名称
 * @param default_value 当环境变量不存在或为空时使用的默认值
 * @return 环境变量字符串值，或传入的默认值
 */
std::string ReadEnvOrDefault(const char* name, std::string default_value) {
    const char* value = std::getenv(name);
    if (value == nullptr || std::string(value).empty()) {
        return default_value;
    }

    return value;
}

/**
 * @brief 读取必须存在的环境变量
 * @param name 环境变量名称
 * @return 环境变量字符串值
 * @throws std::runtime_error 当环境变量不存在或为空时抛出
 */
std::string ReadRequiredEnv(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || std::string(value).empty()) {
        throw std::runtime_error(std::string("missing required environment variable: ") + name);
    }

    return value;
}


/**
 * @brief 读取整数类型环境变量，如果不存在或为空则返回默认值
 * @param name 环境变量名称
 * @param default_value 当环境变量不存在或为空时使用的默认整数值
 * @return 环境变量转换后的整数值，或传入的默认值
 * @throws std::invalid_argument 当环境变量内容无法转换为整数时抛出
 * @throws std::out_of_range 当环境变量数值超出 int 可表示范围时抛出
 */
int ReadIntEnvOrDefault(const char* name, int default_value) {
    const std::string value = ReadEnvOrDefault(name, "");
    if (value.empty()) {
        return default_value;
    }

    return std::stoi(value);
}

/**
 * @brief 读取浮点类型环境变量，如果不存在或为空则返回默认值
 * @param name 环境变量名称
 * @param default_value 当环境变量不存在或为空时使用的默认浮点值
 * @return 环境变量转换后的 double 值，或传入的默认值
 * @throws std::invalid_argument 当环境变量内容无法转换为 double 时抛出
 * @throws std::out_of_range 当环境变量数值超出 double 可表示范围时抛出
 */
double ReadDoubleEnvOrDefault(const char* name, double default_value) {
    const std::string value = ReadEnvOrDefault(name, "");
    if (value.empty()) {
        return default_value;
    }

    return std::stod(value);
}

std::string Trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }

    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::map<std::string, std::string> ReadKeyValueFile(const std::string& config_path) {
    std::ifstream input(config_path);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open config file: " + config_path);
    }

    std::map<std::string, std::string> values;
    std::string line;
    int line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        const auto separator = trimmed.find('=');
        if (separator == std::string::npos) {
            throw std::runtime_error(
                "invalid config line " + std::to_string(line_number) + ": missing '='"
            );
        }

        const std::string key = Trim(trimmed.substr(0, separator));
        const std::string value = Trim(trimmed.substr(separator + 1));
        if (key.empty()) {
            throw std::runtime_error(
                "invalid config line " + std::to_string(line_number) + ": empty key"
            );
        }

        values[key] = value;
    }

    return values;
}

std::string ReadMapRequired(const std::map<std::string, std::string>& values,
                            const char* key) {
    const auto iter = values.find(key);
    if (iter == values.end() || iter->second.empty()) {
        throw std::runtime_error(std::string("missing required config value: ") + key);
    }

    return iter->second;
}

std::string ReadMapOrDefault(const std::map<std::string, std::string>& values,
                             const char* key,
                             std::string default_value) {
    const auto iter = values.find(key);
    if (iter == values.end() || iter->second.empty()) {
        return default_value;
    }

    return iter->second;
}

int ReadMapIntOrDefault(const std::map<std::string, std::string>& values,
                        const char* key,
                        int default_value) {
    const auto value = ReadMapOrDefault(values, key, "");
    if (value.empty()) {
        return default_value;
    }

    return std::stoi(value);
}

double ReadMapDoubleOrDefault(const std::map<std::string, std::string>& values,
                              const char* key,
                              double default_value) {
    const auto value = ReadMapOrDefault(values, key, "");
    if (value.empty()) {
        return default_value;
    }

    return std::stod(value);
}

} // namespace

llm::LLMConfig AppConfig::ToLLMConfig() const {
    llm::LLMConfig config;
    config.api_url = llm_api_url;
    config.api_key = llm_api_key;
    config.model = llm_model;
    config.temperature = llm_temperature;
    config.max_tokens = llm_max_tokens;
    config.timeout_seconds = llm_timeout_seconds;
    return config;
}

AppConfig LoadAppConfigFromEnv() {
    AppConfig config;

    config.runtime_mode = ReadEnvOrDefault(
        "AI_INTERVIEW_RUNTIME_MODE",
        config.runtime_mode
    );
    config.dependency_mode = ReadEnvOrDefault(
        "AI_INTERVIEW_DEPENDENCY_MODE",
        config.dependency_mode
    );

    if (config.dependency_mode == "scripted") {
        config.llm_api_url = ReadEnvOrDefault("AI_INTERVIEW_LLM_API_URL", "");
        config.llm_api_key = ReadEnvOrDefault("AI_INTERVIEW_LLM_API_KEY", "");
    } else {
        config.llm_api_url = ReadRequiredEnv("AI_INTERVIEW_LLM_API_URL");
        config.llm_api_key = ReadRequiredEnv("AI_INTERVIEW_LLM_API_KEY");
    }
    config.llm_model = ReadEnvOrDefault("AI_INTERVIEW_LLM_MODEL", config.llm_model);

    config.llm_temperature = ReadDoubleEnvOrDefault(
        "AI_INTERVIEW_LLM_TEMPERATURE",
        config.llm_temperature
    );

    config.llm_max_tokens = ReadIntEnvOrDefault(
        "AI_INTERVIEW_LLM_MAX_TOKENS",
        config.llm_max_tokens
    );

    config.llm_timeout_seconds = ReadIntEnvOrDefault(
        "AI_INTERVIEW_LLM_TIMEOUT_SECONDS",
        config.llm_timeout_seconds
    );

    config.report_output_dir = ReadEnvOrDefault(
        "AI_INTERVIEW_REPORT_DIR",
        config.report_output_dir
    );
    config.audio_device = ReadEnvOrDefault("AI_INTERVIEW_AUDIO_DEVICE", config.audio_device);
    config.asr_provider = ReadEnvOrDefault("AI_INTERVIEW_ASR_PROVIDER", config.asr_provider);
    config.tts_provider = ReadEnvOrDefault("AI_INTERVIEW_TTS_PROVIDER", config.tts_provider);
    config.input_wav_path = ReadEnvOrDefault("AI_INTERVIEW_INPUT_WAV_PATH", config.input_wav_path);
    config.output_wav_path = ReadEnvOrDefault("AI_INTERVIEW_OUTPUT_WAV_PATH", config.output_wav_path);
    config.scripted_transcripts = ReadEnvOrDefault(
        "AI_INTERVIEW_SCRIPTED_TRANSCRIPTS",
        config.scripted_transcripts
    );
    config.audio_input_sample_rate_hz = ReadIntEnvOrDefault(
        "AI_INTERVIEW_AUDIO_INPUT_SAMPLE_RATE_HZ", config.audio_input_sample_rate_hz);
    config.audio_output_sample_rate_hz = ReadIntEnvOrDefault(
        "AI_INTERVIEW_AUDIO_OUTPUT_SAMPLE_RATE_HZ", config.audio_output_sample_rate_hz);
    config.audio_input_frames_per_buffer = ReadIntEnvOrDefault(
        "AI_INTERVIEW_AUDIO_INPUT_FRAMES_PER_BUFFER", config.audio_input_frames_per_buffer);
    config.audio_output_frames_per_buffer = ReadIntEnvOrDefault(
        "AI_INTERVIEW_AUDIO_OUTPUT_FRAMES_PER_BUFFER", config.audio_output_frames_per_buffer);
    config.volcengine_websocket_url = ReadEnvOrDefault(
        "AI_INTERVIEW_VOLCENGINE_WEBSOCKET_URL", config.volcengine_websocket_url);
    config.volcengine_app_id = ReadEnvOrDefault("AI_INTERVIEW_VOLCENGINE_APP_ID", "");
    config.volcengine_access_key = ReadEnvOrDefault("AI_INTERVIEW_VOLCENGINE_ACCESS_KEY", "");
    config.volcengine_resource_id = ReadEnvOrDefault(
        "AI_INTERVIEW_VOLCENGINE_RESOURCE_ID", config.volcengine_resource_id);
    config.volcengine_app_key = ReadEnvOrDefault("AI_INTERVIEW_VOLCENGINE_APP_KEY", "");
    config.volcengine_speaker = ReadEnvOrDefault(
        "AI_INTERVIEW_VOLCENGINE_SPEAKER", config.volcengine_speaker);
    config.volcengine_operation_timeout_ms = ReadIntEnvOrDefault(
        "AI_INTERVIEW_VOLCENGINE_OPERATION_TIMEOUT_MS", config.volcengine_operation_timeout_ms);
    config.http_host = ReadEnvOrDefault("AI_INTERVIEW_HTTP_HOST", config.http_host);
    config.http_port = ReadIntEnvOrDefault("AI_INTERVIEW_HTTP_PORT", config.http_port);

    return config;
}

AppConfig LoadAppConfigFromFile(const std::string& config_path) {
    AppConfig config;
    const auto values = ReadKeyValueFile(config_path);

    config.runtime_mode = ReadMapOrDefault(
        values,
        "AI_INTERVIEW_RUNTIME_MODE",
        config.runtime_mode
    );
    config.dependency_mode = ReadMapOrDefault(
        values,
        "AI_INTERVIEW_DEPENDENCY_MODE",
        config.dependency_mode
    );
    if (config.dependency_mode == "scripted") {
        config.llm_api_url = ReadMapOrDefault(values, "AI_INTERVIEW_LLM_API_URL", "");
        config.llm_api_key = ReadMapOrDefault(values, "AI_INTERVIEW_LLM_API_KEY", "");
    } else {
        config.llm_api_url = ReadMapRequired(values, "AI_INTERVIEW_LLM_API_URL");
        config.llm_api_key = ReadMapRequired(values, "AI_INTERVIEW_LLM_API_KEY");
    }
    config.llm_model = ReadMapOrDefault(
        values,
        "AI_INTERVIEW_LLM_MODEL",
        config.llm_model
    );
    config.llm_temperature = ReadMapDoubleOrDefault(
        values,
        "AI_INTERVIEW_LLM_TEMPERATURE",
        config.llm_temperature
    );
    config.llm_max_tokens = ReadMapIntOrDefault(
        values,
        "AI_INTERVIEW_LLM_MAX_TOKENS",
        config.llm_max_tokens
    );
    config.llm_timeout_seconds = ReadMapIntOrDefault(
        values,
        "AI_INTERVIEW_LLM_TIMEOUT_SECONDS",
        config.llm_timeout_seconds
    );
    config.report_output_dir = ReadMapOrDefault(
        values,
        "AI_INTERVIEW_REPORT_DIR",
        config.report_output_dir
    );
    config.audio_device = ReadMapOrDefault(
        values,
        "AI_INTERVIEW_AUDIO_DEVICE",
        config.audio_device
    );
    config.asr_provider = ReadMapOrDefault(
        values,
        "AI_INTERVIEW_ASR_PROVIDER",
        config.asr_provider
    );
    config.tts_provider = ReadMapOrDefault(
        values,
        "AI_INTERVIEW_TTS_PROVIDER",
        config.tts_provider
    );
    config.input_wav_path = ReadMapOrDefault(
        values,
        "AI_INTERVIEW_INPUT_WAV_PATH",
        config.input_wav_path
    );
    config.output_wav_path = ReadMapOrDefault(
        values,
        "AI_INTERVIEW_OUTPUT_WAV_PATH",
        config.output_wav_path
    );
    config.scripted_transcripts = ReadMapOrDefault(
        values,
        "AI_INTERVIEW_SCRIPTED_TRANSCRIPTS",
        config.scripted_transcripts
    );
    config.audio_input_sample_rate_hz = ReadMapIntOrDefault(
        values, "AI_INTERVIEW_AUDIO_INPUT_SAMPLE_RATE_HZ", config.audio_input_sample_rate_hz);
    config.audio_output_sample_rate_hz = ReadMapIntOrDefault(
        values, "AI_INTERVIEW_AUDIO_OUTPUT_SAMPLE_RATE_HZ", config.audio_output_sample_rate_hz);
    config.audio_input_frames_per_buffer = ReadMapIntOrDefault(
        values, "AI_INTERVIEW_AUDIO_INPUT_FRAMES_PER_BUFFER", config.audio_input_frames_per_buffer);
    config.audio_output_frames_per_buffer = ReadMapIntOrDefault(
        values, "AI_INTERVIEW_AUDIO_OUTPUT_FRAMES_PER_BUFFER", config.audio_output_frames_per_buffer);
    config.volcengine_websocket_url = ReadMapOrDefault(
        values, "AI_INTERVIEW_VOLCENGINE_WEBSOCKET_URL", config.volcengine_websocket_url);
    config.volcengine_app_id = ReadMapOrDefault(values, "AI_INTERVIEW_VOLCENGINE_APP_ID", "");
    config.volcengine_access_key = ReadMapOrDefault(values, "AI_INTERVIEW_VOLCENGINE_ACCESS_KEY", "");
    config.volcengine_resource_id = ReadMapOrDefault(
        values, "AI_INTERVIEW_VOLCENGINE_RESOURCE_ID", config.volcengine_resource_id);
    config.volcengine_app_key = ReadMapOrDefault(values, "AI_INTERVIEW_VOLCENGINE_APP_KEY", "");
    config.volcengine_speaker = ReadMapOrDefault(
        values, "AI_INTERVIEW_VOLCENGINE_SPEAKER", config.volcengine_speaker);
    config.volcengine_operation_timeout_ms = ReadMapIntOrDefault(
        values, "AI_INTERVIEW_VOLCENGINE_OPERATION_TIMEOUT_MS", config.volcengine_operation_timeout_ms);
    config.http_host = ReadMapOrDefault(
        values,
        "AI_INTERVIEW_HTTP_HOST",
        config.http_host
    );
    config.http_port = ReadMapIntOrDefault(
        values,
        "AI_INTERVIEW_HTTP_PORT",
        config.http_port
    );

    return config;
}

AppConfig LoadAppConfig(const std::string& config_path) {
    if (!config_path.empty()) {
        return LoadAppConfigFromFile(config_path);
    }

    if (std::filesystem::exists(kDefaultLocalConfigPath)) {
        return LoadAppConfigFromFile(kDefaultLocalConfigPath);
    }

    return LoadAppConfigFromEnv();
}

} // namespace config
} // namespace interview
