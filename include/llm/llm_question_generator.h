#pragma once

#include <memory>

#include "core/interview_ports.h"
#include "llm/llm_client.h"

namespace interview {
namespace llm {

/**
 * @brief LLM 题目生成器
 * @note 实现 core::QuestionGenerator，将 LLM 返回内容转换成 InterviewQuestion 列表
 */
class LLMQuestionGenerator final : public core::QuestionGenerator {
public:
    /**
     * @brief 构造 LLM 题目生成器
     * @param client LLM 客户端的引用，这里需要注意，调用方必须保证生命周期长于这个对象
     */
    explicit LLMQuestionGenerator(LLMClient& client);

    ~LLMQuestionGenerator() override;

    LLMQuestionGenerator(const LLMQuestionGenerator&) = delete;
    LLMQuestionGenerator& operator=(const LLMQuestionGenerator&) = delete;

    LLMQuestionGenerator(LLMQuestionGenerator&&) noexcept;
    LLMQuestionGenerator& operator=(LLMQuestionGenerator&&) noexcept;

    /**
     * @brief 重写生成面试题目的抽象接口
     */
    std::vector<core::InterviewQuestion> GenerateDefault(int min_questions) override;

    std::vector<core::InterviewQuestion> GenerateFromResume(
            const std::string& resume_text,
            int min_questions
    ) override;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace llm
} // namespace interview
