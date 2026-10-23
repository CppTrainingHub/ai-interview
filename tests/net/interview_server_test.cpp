#include "config/app_config.h"
#include "net/http_interview_server.h"
#include "net/interview_server.h"

#include <cassert>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

namespace {

/**
 * @brief 构造测试使用的应用配置。
 * @return AppConfig 测试配置。
 * @note 当前测试只覆盖不触发真实 LLM 调用的错误路径和健康检查路径。
 */
interview::config::AppConfig MakeTestConfig() {
    interview::config::AppConfig config;
    config.dependency_mode = "scripted";
    config.report_output_dir = "reports";
    return config;
}

/**
 * @brief 验证健康检查接口返回可用状态。
 */
void TestHealthResponse() {
    interview::net::InterviewServer server(MakeTestConfig());

    const auto response = server.HandleHealth();

    assert(response.find("\"ok\": true") != std::string::npos);
    assert(response.find("\"service\": \"ai-interview-net\"") != std::string::npos);
    assert(response.find("\"status\": \"ready\"") != std::string::npos);
}

/**
 * @brief 验证提交回答时找不到会话会返回错误响应。
 */
void TestAnswerWithUnknownSession() {
    interview::net::InterviewServer server(MakeTestConfig());

    const auto response = server.HandleAnswer(R"({
        "session_id": "not-exist",
        "answer": "RAII uses object lifetime to manage resources."
    })");

    assert(response.find("\"ok\": false") != std::string::npos);
    assert(response.find("session not found: not-exist") != std::string::npos);
}

/**
 * @brief 验证生成报告时找不到会话会返回错误响应。
 */
void TestReportWithUnknownSession() {
    interview::net::InterviewServer server(MakeTestConfig());

    const auto response = server.HandleReport(R"({
        "session_id": "not-exist"
    })");

    assert(response.find("\"ok\": false") != std::string::npos);
    assert(response.find("session not found: not-exist") != std::string::npos);
}

/**
 * @brief 验证启动面试时非法题目数量会返回错误响应。
 * @note 该路径在创建 LLM 会话后、调用题目生成前返回，避免依赖真实模型服务。
 */
void TestStartWithInvalidQuestionCount() {
    interview::net::InterviewServer server(MakeTestConfig());

    const auto response = server.HandleStart(R"({
        "candidate_name": "Alice",
        "min_questions": 0
    })");

    assert(response.find("\"ok\": false") != std::string::npos);
    assert(response.find("min_questions must be positive") != std::string::npos);
}

/**
 * @brief 验证非法 JSON 会被转换为错误响应。
 */
void TestInvalidJsonReturnsError() {
    interview::net::InterviewServer server(MakeTestConfig());

    const auto response = server.HandleAnswer("not json");

    assert(response.find("\"ok\": false") != std::string::npos);
    assert(response.find("\"error\"") != std::string::npos);
}

/**
 * @brief 从原始 HTTP 响应中提取 body 文本。
 * @param response 原始 HTTP 响应。
 * @return HTTP body 文本。
 */
std::string ExtractHttpBody(const std::string& response) {
    const auto body_start = response.find("\r\n\r\n");
    assert(body_start != std::string::npos);
    return response.substr(body_start + 4);
}

/**
 * @brief 构造一个带 Content-Length 的 HTTP 请求。
 * @param method HTTP 方法。
 * @param path HTTP 路径。
 * @param body 请求体。
 * @return 原始 HTTP 请求文本。
 */
std::string MakeHttpRequest(const std::string& method,
                            const std::string& path,
                            const std::string& body = "") {
    return method + " " + path + " HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" +
        body;
}

/**
 * @brief 验证 HTTP 健康检查路由会转发到 InterviewServer。
 */
void TestHttpHealthRoute() {
    interview::net::HttpInterviewServer server(MakeTestConfig());

    const auto response = server.HandleRawRequest(MakeHttpRequest("GET", "/health"));

    assert(response.find("HTTP/1.1 200 OK") != std::string::npos);
    assert(response.find("Content-Type: application/json") != std::string::npos);
    const auto body = nlohmann::json::parse(ExtractHttpBody(response));
    assert(body.value("ok", false));
    assert(body.value("service", "") == "ai-interview-net");
}

/**
 * @brief 验证 HTTP 提交回答路由会返回业务错误响应和 400 状态。
 */
void TestHttpAnswerRouteBusinessError() {
    interview::net::HttpInterviewServer server(MakeTestConfig());
    const auto body = R"({"session_id":"not-exist","answer":"测试回答"})";

    const auto response = server.HandleRawRequest(
        MakeHttpRequest("POST", "/interview/answer", body));

    assert(response.find("HTTP/1.1 400 Bad Request") != std::string::npos);
    const auto json = nlohmann::json::parse(ExtractHttpBody(response));
    assert(!json.value("ok", true));
    assert(json.value("error", "").find("session not found") != std::string::npos);
}

/**
 * @brief 验证未知路由返回 404。
 */
void TestHttpUnknownRoute() {
    interview::net::HttpInterviewServer server(MakeTestConfig());

    const auto response = server.HandleRawRequest(MakeHttpRequest("POST", "/不存在", "{}"));

    assert(response.find("HTTP/1.1 404 Not Found") != std::string::npos);
    const auto json = nlohmann::json::parse(ExtractHttpBody(response));
    assert(!json.value("ok", true));
}

/**
 * @brief 验证网络服务在离线 scripted 模式下可以完整跑完面试并生成报告。
 */
void TestScriptedInterviewFullFlow() {
    auto config = MakeTestConfig();
    config.report_output_dir =
        (std::filesystem::temp_directory_path() / "ai_interview_net_test").string();
    interview::net::InterviewServer server(config);

    auto start = nlohmann::json::parse(server.HandleStart(R"({
        "candidate_name": "Net Alice",
        "min_questions": 1
    })"));
    assert(start.value("ok", false));
    const auto session_id = start.value("session_id", "");
    assert(!session_id.empty());
    assert(start.value("action_type", "") == "speak_intro");

    auto intro = nlohmann::json::parse(server.HandleAnswer(
        "{\"session_id\":\"" + session_id + "\",\"answer\":\"I am ready.\"}"));
    assert(intro.value("ok", false));
    assert(intro.value("action_type", "") == "ask_question");

    auto finish = nlohmann::json::parse(server.HandleAnswer(
        "{\"session_id\":\"" + session_id +
        "\",\"answer\":\"RAII binds resource ownership to object lifetime and destructors.\"}"));
    assert(finish.value("ok", false));
    assert(finish.value("action_type", "") == "finish");

    auto report = nlohmann::json::parse(server.HandleReport(
        "{\"session_id\":\"" + session_id + "\"}"));
    assert(report.value("ok", false));
    assert(std::filesystem::exists(report.value("report_path", "")));
}

} // namespace

int main() {
    TestHealthResponse();
    TestAnswerWithUnknownSession();
    TestReportWithUnknownSession();
    TestStartWithInvalidQuestionCount();
    TestInvalidJsonReturnsError();
    TestHttpHealthRoute();
    TestHttpAnswerRouteBusinessError();
    TestHttpUnknownRoute();
    TestScriptedInterviewFullFlow();
    return 0;
}
