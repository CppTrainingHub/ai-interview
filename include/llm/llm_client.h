#pragma once

#include <memory>
#include <string>
#include <vector>

#include "llm/llm_config.h"

namespace interview {
namespace llm {
    
/**
 * @brief LLM 对话消息
 */
struct LLMMessage {
    std::string role;
    std::string content;
};

/**
 * @brief LLM 客户端接口
 * @note 只负责发送消息并返回模型文本，不处理面试业务语义
 */
class LLMClient {
public:
    virtual ~LLMClient() = default;

    /**
     * @brief 发送单条用户消息
     * @param message 用户消息
     * @param system_prompt 系统提示词
     * @return LLM 返回的 assistant 文本
     */
    virtual std::string SendMessage(const std::string& message,
                                    const std::string& system_prompt) = 0;

    /**
     * @brief 发送多轮对话消息
     * @param messages 消息列表
     * @return LLM 返回的 assistant 文本
     */
    virtual std::string SendConversation(const std::vector<LLMMessage>& messages) = 0;
};

/**
 * @brief OpenAI 兼容 LLM 客户端
 * @note 通过 Http Post 调用OpenAI-Compatible API
 */
class OpenAICompatibleLLMClient final : public LLMClient {
public:
    /**
     * @brief 构造 LLM 客户端
     * @param config LLM HTTP 调用配置
     */
    explicit OpenAICompatibleLLMClient(LLMConfig config);

    /**
     * @brief 析构
     */
    ~OpenAICompatibleLLMClient() override;

    /**
     * @brief 拷贝构造和移动构造
     */
    OpenAICompatibleLLMClient(const OpenAICompatibleLLMClient&) = delete;
    OpenAICompatibleLLMClient& operator=(const OpenAICompatibleLLMClient&) = delete;

    OpenAICompatibleLLMClient(OpenAICompatibleLLMClient&&) noexcept;
    OpenAICompatibleLLMClient& operator=(OpenAICompatibleLLMClient&&) noexcept;

    /**
     * @brief 重写发送消息与发送对话的接口
     */
    std::string SendMessage(const std::string& message,
                            const std::string& system_prompt) override;

    std::string SendConversation(const std::vector<LLMMessage>& messages) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};



} // namespace llm
} // namespace interview
