#include <stdexcept>
#include <string>
#include <utility>

#include "llm/llm_question_generator.h"
#include "llm/llm_response_parser.h"

namespace interview {
namespace llm {
namespace {

/**
 * @brief 构造题目生成系统提示词
 * @param min_questions 期望生成的最少问题数量
 * @return 系统提示词
 */
std::string BuildQuestionSystemPrompt(int min_questions) {
    return "你是一位资深 C++ 技术面试官。请生成至少 " +
           std::to_string(min_questions) +
           " 个技术面试问题，并只返回 JSON 数组。"
           "每个问题包含 question、category、level、key_points、expected_keywords 字段。";
}

/**
 * @brief 构造默认 C++ 岗位描述
 * @return 默认题目生成所需的岗位背景文本
 */
std::string BuildDefaultResumeText() {
    return "C++开发工程师岗位，要求掌握 C++17、内存管理、RAII、STL、"
           "面向对象、多线程、工程实践和基础设计模式。";
}

/**
 * @brief 校验题目数量参数
 * @param min_questions 期望生成的最少问题数量
 */
void ValidateMinQuestions(int min_questions) {
    if (min_questions <= 0) {
        throw std::runtime_error("min_questions must be positive");
    }
}

} // namespace

class LLMQuestionGenerator::Impl {
public:
    /**
     * @brief 构造题目生成器实现
     * @param client_ref LLM 客户端引用
     */
    explicit Impl(LLMClient& client_ref)
        : client(client_ref) {}

    /**
     * @brief 根据简历文本生成面试题
     * @param resume_text 简历文本
     * @param min_questions 最少题目数量
     * @return 面试题列表
     */
    std::vector<core::InterviewQuestion> GenerateFromResume(const std::string& resume_text,
                                                            int min_questions) {
        ValidateMinQuestions(min_questions);

        const std::string user_prompt =
            "候选人简历或背景如下：\n\n" + resume_text + "\n\n请根据以上内容生成 C++ 面试题。";

        const auto response = client.SendMessage(
            user_prompt,
            BuildQuestionSystemPrompt(min_questions)
        );

        return ParseQuestions(response);
    }

    /**
     * @brief 生成默认 C++ 面试题
     * @param min_questions 最少题目数量
     * @return 面试题列表
     */
    std::vector<core::InterviewQuestion> GenerateDefault(int min_questions) {
        return GenerateFromResume(BuildDefaultResumeText(), min_questions);
    }

private:
    LLMClient& client;
};

LLMQuestionGenerator::LLMQuestionGenerator(LLMClient& client)
    : pImpl(std::make_unique<Impl>(client)) {}

LLMQuestionGenerator::~LLMQuestionGenerator() = default;
LLMQuestionGenerator::LLMQuestionGenerator(LLMQuestionGenerator&&) noexcept = default;
LLMQuestionGenerator& LLMQuestionGenerator::operator=(LLMQuestionGenerator&&) noexcept = default;

std::vector<core::InterviewQuestion> LLMQuestionGenerator::GenerateDefault(int min_questions) {
    return pImpl->GenerateDefault(min_questions);
}

std::vector<core::InterviewQuestion> LLMQuestionGenerator::GenerateFromResume(
    const std::string& resume_text,
    int min_questions) {
    return pImpl->GenerateFromResume(resume_text, min_questions);
}

} // namespace llm
} // namespace interview
