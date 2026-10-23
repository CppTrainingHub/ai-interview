#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "llm/llm_response_parser.h"

namespace interview {
namespace llm {
namespace {

/**
 * @brief 从 LLM 回复中提取 JSON 片段
 * @param response LLM 原始回复文本，可能包含 Markdown 包裹或解释性文字
 * @param open JSON 起始字符，例如 '[' 或 '{'
 * @param close JSON 结束字符，例如 ']' 或 '}'
 * @return 提取出的 JSON 字符串
 * @throws std::runtime_error 当找不到合法 JSON 边界时抛出
 */
std::string ExtractJsonBlock(const std::string& response, char open, char close) {
    const auto begin = response.find(open);
    const auto end = response.rfind(close);

    if (begin == std::string::npos || end == std::string::npos || end < begin) {
        throw std::runtime_error("LLM response does not contain a valid JSON block");
    }

    return response.substr(begin, end - begin + 1);
}

/**
 * @brief 将分数限制在 0 到 100 之间
 * @param score 原始分数
 * @return 修正后的合法分数
 */
int ClampScore(int score) {
    return std::max(0, std::min(100, score));
}

/**
 * @brief 从 JSON 对象中读取字符串数组
 * @param object JSON 对象
 * @param key 字段名
 * @return 字符串数组；字段不存在或格式不对时返回空数组
 */
std::vector<std::string> ReadStringArray(const nlohmann::json& object, const char* key) {
    std::vector<std::string> values;

    if (!object.contains(key) || !object.at(key).is_array()) {
        return values;
    }

    for (const auto& item : object.at(key)) {
        if (item.is_string()) {
            values.push_back(item.get<std::string>());
        }
    }

    return values;
}

} // namespace

std::vector<core::InterviewQuestion> ParseQuestions(const std::string& response) {
    const auto json_text = ExtractJsonBlock(response, '[', ']');
    const auto root = nlohmann::json::parse(json_text);

    if (!root.is_array() || root.empty()) {
        throw std::runtime_error("LLM questions response must be a non-empty JSON array");
    }

    std::vector<core::InterviewQuestion> questions;
    for (const auto& item : root) {
        if (!item.is_object()) {
            continue;
        }

        core::InterviewQuestion question;
        question.question = item.value("question", "");
        question.category = item.value("category", "general");
        question.level = item.value("level", "intermediate");
        question.key_points = ReadStringArray(item, "key_points");
        question.expected_keywords = ReadStringArray(item, "expected_keywords");

        if (!question.question.empty()) {
            questions.push_back(std::move(question));
        }
    }

    if (questions.empty()) {
        throw std::runtime_error("LLM questions response contains no valid question");
    }

    return questions;
}

core::AnswerEvaluation ParseEvaluation(const std::string& response) {
    const auto json_text = ExtractJsonBlock(response, '{', '}');
    const auto root = nlohmann::json::parse(json_text);

    core::AnswerEvaluation evaluation;
    evaluation.score = ClampScore(root.value("score", 0));
    evaluation.feedback = root.value("feedback", "");
    evaluation.need_followup = root.value("need_followup", false);
    evaluation.followup_question = root.value("followup_question", "");

    if (evaluation.followup_question.empty()) {
        evaluation.need_followup = false;
    }

    return evaluation;
}

std::string ParseSummaryText(const std::string& response) {
    try {
        const auto json_text = ExtractJsonBlock(response, '{', '}');
        const auto root = nlohmann::json::parse(json_text);

        std::string result = root.value("summary", "");

        const std::string recommendation = root.value("recommendation", "");
        if (!recommendation.empty()) {
            result += "\n录用建议：" + recommendation;
        }

        const std::string hiring_level = root.value("hiring_level", "");
        if (!hiring_level.empty()) {
            result += "\n建议级别：" + hiring_level;
        }

        if (!result.empty()) {
            return result;
        }
    } catch (...) {
        // 非 JSON 总结可以直接作为纯文本返回。
    }

    return response;
}

} // namespace llm
} // namespace interview
