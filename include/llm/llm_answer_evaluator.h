#pragma once

#include <memory>
#include <string>

#include "core/interview_ports.h"
#include "llm/llm_client.h"

namespace interview {
namespace llm {

/**
 * @brief LLM 回答评分器
 * @note 实现 core::AnswerEvaluator，将 LLM 评分 JSON 转换成 AnswerEvaluation。
 */
class LLMAnswerEvaluator final : public core::AnswerEvaluator {
public:
    /**
     * @brief 构造函数
     * @param client LLM 客户端的引用，调用方必须保证生命周期长于本对象
     */
    explicit LLMAnswerEvaluator(LLMClient& client);

    /**
     * @brief 析构函数
     */
    ~LLMAnswerEvaluator() override;

    LLMAnswerEvaluator(const LLMAnswerEvaluator&) = delete;
    LLMAnswerEvaluator& operator=(const LLMAnswerEvaluator&) = delete;

    LLMAnswerEvaluator(LLMAnswerEvaluator&&) noexcept;
    LLMAnswerEvaluator& operator=(LLMAnswerEvaluator&&) noexcept;

    /**
     * @brief 对候选人回答进行 LLM 评分
     * @param question 当前问题
     * @param answer 候选人回答
     * @param is_followup 当前回答是否来自追问
     * @return 评分结果
     */
    core::AnswerEvaluation Evaluate(const core::InterviewQuestion& question,
                                    const std::string& answer,
                                    bool is_followup) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace llm
} // namespace interview
