#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "net/interview_controller.h"
#include "net/interview_net_models.h"
#include "net/interview_server.h"

namespace interview {
namespace net {
namespace {

/**
 * @brief 从 JSON 字符串解析启动面试请求
 * @param json_body 请求 JSON 字符串
 * @return StartInterviewRequest
 */
StartInterviewRequest ParseStartRequest(const std::string& json_body) {
    const auto json = nlohmann::json::parse(json_body);

    StartInterviewRequest request;
    request.candidate_name = json.value("candidate_name", "Candidate");
    request.resume_path = json.value("resume_path", "");
    request.min_questions = json.value("min_questions", 5);
    return request;
}

/**
 * @brief 从 JSON 字符串解析提交回答请求
 * @param json_body 请求 JSON 字符串
 * @return SubmitAnswerRequest
 */
SubmitAnswerRequest ParseAnswerRequest(const std::string& json_body) {
    const auto json = nlohmann::json::parse(json_body);

    SubmitAnswerRequest request;
    request.session_id = json.value("session_id", "");
    request.answer = json.value("answer", "");
    return request;
}

/**
 * @brief 从 JSON 字符串解析生成报告请求
 * @param json_body 请求 JSON 字符串
 * @return GenerateReportRequest
 */
GenerateReportRequest ParseReportRequest(const std::string& json_body) {
    const auto json = nlohmann::json::parse(json_body);

    GenerateReportRequest request;
    request.session_id = json.value("session_id", "");
    return request;
}

/**
 * @brief 将网络响应转换为 JSON 字符串
 * @param response 网络层统一响应
 * @return JSON 字符串
 */
std::string SerializeResponse(const InterviewNetResponse& response) {
    nlohmann::json json;
    json["ok"] = response.ok;
    json["finished"] = response.finished;
    json["error"] = response.error;
    json["session_id"] = response.session_id;
    json["action_type"] = response.action_type;
    json["text"] = response.text;
    json["question_index"] = response.question_index;
    json["report_path"] = response.report_path;
    return json.dump(2);
}

/**
 * @brief 构造 JSON 解析失败响应
 * @param message 错误信息
 * @return JSON 字符串
 */
std::string SerializeError(const std::string& message) {
    InterviewNetResponse response;
    response.ok = false;
    response.error = message;
    return SerializeResponse(response);
}

} // namespace

class InterviewServer::Impl {
public:
    explicit Impl(config::AppConfig app_config)
        : controller(std::move(app_config)) {}

    std::string HandleHealth() const {
        nlohmann::json json;
        json["ok"] = true;
        json["service"] = "ai-interview-net";
        json["status"] = "ready";
        return json.dump(2);
    }

    std::string HandleStart(const std::string& json_body) {
        try {
            return SerializeResponse(
                controller.StartInterview(ParseStartRequest(json_body)));
        } catch (const std::exception& e) {
            return SerializeError(e.what());
        }
    }

    std::string HandleAnswer(const std::string& json_body) {
        try {
            return SerializeResponse(
                controller.SubmitAnswer(ParseAnswerRequest(json_body)));
        } catch (const std::exception& e) {
            return SerializeError(e.what());
        }
    }

    std::string HandleReport(const std::string& json_body) {
        try {
            return SerializeResponse(
                controller.GenerateReport(ParseReportRequest(json_body)));
        } catch (const std::exception& e) {
            return SerializeError(e.what());
        }
    }

private:
    InterviewController controller;
};

InterviewServer::InterviewServer(config::AppConfig config)
    : pImpl(std::make_unique<Impl>(std::move(config))) {}

InterviewServer::~InterviewServer() = default;

InterviewServer::InterviewServer(InterviewServer&&) noexcept = default;

InterviewServer& InterviewServer::operator=(InterviewServer&&) noexcept = default;

std::string InterviewServer::HandleHealth() const {
    return pImpl->HandleHealth();
}

std::string InterviewServer::HandleStart(const std::string& json_body) {
    return pImpl->HandleStart(json_body);
}

std::string InterviewServer::HandleAnswer(const std::string& json_body) {
    return pImpl->HandleAnswer(json_body);
}

std::string InterviewServer::HandleReport(const std::string& json_body) {
    return pImpl->HandleReport(json_body);
}

} // namespace net
} // namespace interview