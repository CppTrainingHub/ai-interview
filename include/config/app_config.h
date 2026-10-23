#pragma once

#include <string>

#include "llm/llm_config.h"

namespace interview {
namespace config {

/**
 * @brief 应用启动配置
 * @note 只负责保存启动参数和外部服务配置，不包含业务流程逻辑
 */
struct AppConfig {
    std::string runtime_mode = "text";
    std::string dependency_mode = "openai";

    std::string llm_api_url;
    std::string llm_api_key;
    /**
     * @brief 默认 LLM 模型名称
     * @note 可通过配置文件或 AI_INTERVIEW_LLM_MODEL 环境变量覆盖
     */
    std::string llm_model = "deepseek-v4-flash";

    double llm_temperature = 0.2;
    int llm_max_tokens = 2048;
    int llm_timeout_seconds = 60;

    std::string report_output_dir = "reports";

    std::string audio_device = "null";
    std::string asr_provider = "scripted";
    std::string tts_provider = "silent_pcm";
    std::string input_wav_path;
    std::string output_wav_path = "reports/audio_output.wav";
    std::string scripted_transcripts;

    int audio_input_sample_rate_hz = 16000;
    int audio_output_sample_rate_hz = 24000;
    int audio_input_frames_per_buffer = 3200;
    int audio_output_frames_per_buffer = 4800;

    std::string volcengine_websocket_url =
        "wss://openspeech.bytedance.com/api/v3/realtime/dialogue";
    std::string volcengine_app_id;
    std::string volcengine_access_key;
    std::string volcengine_resource_id = "volc.speech.dialog";
    std::string volcengine_app_key;
    std::string volcengine_speaker = "zh_male_yunzhou_jupiter_bigtts";
    int volcengine_operation_timeout_ms = 30000;

    std::string http_host = "127.0.0.1";
    int http_port = 8080;

    /**
     * @brief 转换为 LLM 模块使用的配置
     * @return LLMConfig 配置对象
     */
    llm::LLMConfig ToLLMConfig() const;
};

/**
 * @brief 从环境变量读取应用配置
 * @return 应用配置对象
 * @throws std::runtime_error 当必要配置缺失时抛出
 */
AppConfig LoadAppConfigFromEnv();

/**
 * @brief 从 key=value 配置文件读取应用配置
 * @param config_path 配置文件路径
 * @return 应用配置对象
 * @throws std::runtime_error 当配置文件不可读或必要配置缺失时抛出
 */
AppConfig LoadAppConfigFromFile(const std::string& config_path);

/**
 * @brief 加载应用配置
 * @param config_path 显式指定的配置文件路径；为空时优先读取默认本地配置，再回退环境变量
 * @return 应用配置对象
 * @note 默认本地配置路径为 config/app.local.env，该文件应被 git 忽略。
 */
AppConfig LoadAppConfig(const std::string& config_path = "");

} // namespace config
} // namespace interview
