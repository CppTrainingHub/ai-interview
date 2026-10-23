#pragma once

#include <string>

namespace interview {
namespace llm {
    
/**
 * @brief LLM 调用配置
 * @note 这里只保存 LLM Http 调用需要的配置，不包含音频、WebSocket、UI等外层配置
 */
struct LLMConfig {
    std::string api_url;
    std::string api_key;
    std::string model;
    double temperature = 0.7;
    int max_tokens = 4096;
    int timeout_seconds = 60;
};

} // namespace llm
} // namespace interview
