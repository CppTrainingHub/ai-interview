#include <sstream>
#include <string>
#include <utility>

#include "llm/llm_answer_evaluator.h"
#include "llm/llm_response_parser.h"

namespace interview {
namespace llm {
namespace {

/**
 * @brief 构造评分系统提示词
 * @param is_followup 当前回答是否是追问回答
 * @return 系统提示词
 */
std::string BuildEvaluationSystemPrompt(bool is_followup) {
    std::string prompt =
        "你是一位严格、客观的 C++ 技术面试官。"
        "请根据问题和候选人回答进行评分，并只返回 JSON 对象。"
        "字段包括 score、feedback、need_followup、followup_question。"
        "score 必须在 0 到 100 之间。";

    if (is_followup) {
        prompt += " 当前回答是追问回答，need_followup 必须为 false。";
    }

    return prompt;
}

/**
 * @brief 把 InterviewQuestion 转换为 prompt 文本
 * @param question 面试题
 * @return 可发送给 LLM 的题目信息文本
 */
std::string FormatQuestionForPrompt(const core::InterviewQuestion& question) {
    std::ostringstream oss;
    oss << "问题：" << question.question << "\n";
    oss << "类别：" << question.category << "\n";
    oss << "难度：" << question.level << "\n";

    if (!question.key_points.empty()) {
        oss << "考察点：";
        for (const auto& point : question.key_points) {
            oss << point << " ";
        }
        oss << "\n";
    }

    if (!question.expected_keywords.empty()) {
        oss << "期望关键词：";
        for (const auto& keyword : question.expected_keywords) {
            oss << keyword << " ";
        }
        oss << "\n";
    }

    return oss.str();
}

/**
 * @brief 构造评分失败时的默认结果
 * @return 默认评分结果
 */
core::AnswerEvaluation BuildFallbackEvaluation() {
    core::AnswerEvaluation evaluation;
    evaluation.score = 50;
    evaluation.feedback = "LLM 评分失败，使用默认中等分数。";
    evaluation.need_followup = false;
    return evaluation;
}

} // namespace

class LLMAnswerEvaluator::Impl {
public:
    /**
     * @brief 构造回答评分器实现
     * @param client_ref LLM 客户端引用
     */
    explicit Impl(LLMClient& client_ref)
        : client(client_ref) {}

    /**
     * @brief 评估候选人回答
     * @param question 当前面试题
     * @param answer 候选人回答
     * @param is_followup 当前回答是否是追问回答
     * @return 评分结果
     */
    core::AnswerEvaluation Evaluate(const core::InterviewQuestion& question,
                                    const std::string& answer,
                                    bool is_followup) {
        const std::string user_prompt =
            FormatQuestionForPrompt(question) + "\n候选人回答：\n" + answer;

        try {
            auto evaluation = ParseEvaluation(
                client.SendMessage(user_prompt, BuildEvaluationSystemPrompt(is_followup))
            );

            if (is_followup) {
                evaluation.need_followup = false;
                evaluation.followup_question.clear();
            }

            return evaluation;
        } catch (const std::exception&) {
            return BuildFallbackEvaluation();
        }
    }

private:
    LLMClient& client;
};

LLMAnswerEvaluator::LLMAnswerEvaluator(LLMClient& client)
    : pImpl(std::make_unique<Impl>(client)) {}

LLMAnswerEvaluator::~LLMAnswerEvaluator() = default;
LLMAnswerEvaluator::LLMAnswerEvaluator(LLMAnswerEvaluator&&) noexcept = default;
LLMAnswerEvaluator& LLMAnswerEvaluator::operator=(LLMAnswerEvaluator&&) noexcept = default;

core::AnswerEvaluation LLMAnswerEvaluator::Evaluate(const core::InterviewQuestion& question,
                                                    const std::string& answer,
                                                    bool is_followup) {
    return pImpl->Evaluate(question, answer, is_followup);
}

} // namespace llm
} // namespace interview
