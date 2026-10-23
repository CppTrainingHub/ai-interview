#pragma once

#include <string>
#include <vector>

#include "core/interview_models.h"

namespace interview {
namespace llm {

/**
 * @brief 从 LLM 文本响应中解析面试题列表
 * @param response LLM 原始文本响应，允许包含 Markdown 代码块
 * @return 面试题列表
 * @throws std::runtime_error 当 JSON 无法解析或者题目为空时抛出异常
 */
std::vector<core::InterviewQuestion> ParseQuestions(const std::string& response);

/**
 * @brief 从 LLM 文本响应中解析回答评分
 * @param response LLM 原始文本响应，允许包含 Markdown 代码块
 * @return 回答评分结果
 * @throws std::runtime_error 当 JSON 无法解析时抛出异常
 */
core::AnswerEvaluation ParseEvaluation(const std::string& response);

/**
 * @brief 从 LLM 文本响应中解析总结文本
 * @param response LLM 原始文本响应，允许是纯文本或者 JSON
 * @return 面试总结文本
 */
std::string ParseSummaryText(const std::string& response);

} // namespace llm
} // namespace interview
