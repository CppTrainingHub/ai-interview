#pragma once

#include <string>

namespace interview {
namespace net {

/**
 * @brief 网络层启动面试请求模型
 * @note 该结构只表示外部请求数据，不包含 LLM、PDF 或报告实现细节
 */
struct StartInterviewRequest {
    std::string candidate_name = "Candidate";
    std::string resume_path;
    int min_questions = 5;
};

/**
 * @brief 网络层提交回答请求模型
 * @note session_id 由 StartInterview 返回，answer 可以来自 HTTP、WebSocket 或 ASR 文本
 */
struct SubmitAnswerRequest {
    std::string session_id;
    std::string answer;
};

/**
 * @brief 网络层生成报告请求模型
 */
struct GenerateReportRequest {
    std::string session_id;
};

/**
 * @brief 网络层统一响应模型
 * @note action_type 对应 core::InterviewActionType 的字符串形式
 */
struct InterviewNetResponse {
    bool ok = true;
    bool finished = false;
    std::string error;
    std::string session_id;
    std::string action_type;
    std::string text;
    int question_index = 0;
    std::string report_path;
};

} // namespace net
} // namespace interview