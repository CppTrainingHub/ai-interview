#pragma once

#include <memory>
#include <string>

#include "config/app_config.h"
#include "net/interview_net_models.h"

namespace interview {
namespace net {

/**
 * @brief 面试网络控制器，负责把外部请求转换为 core 面试流程调用
 * @note 该类不实现真实 HTTP 监听，只管理 session 生命周期和业务调用边界
 */
class InterviewController {
public:
    /**
     * @brief 构造网络控制器
     * @param config 应用配置，包含 LLM 配置和报告输出目录
     */
    explicit InterviewController(config::AppConfig config);

    ~InterviewController();

    InterviewController(const InterviewController&) = delete;
    InterviewController& operator=(const InterviewController&) = delete;

    InterviewController(InterviewController&&) noexcept;
    InterviewController& operator=(InterviewController&&) noexcept;

    /**
     * @brief 创建新面试会话并返回开场动作
     * @param request 启动面试请求
     * @return 网络层统一响应
     */
    InterviewNetResponse StartInterview(const StartInterviewRequest& request);

    /**
     * @brief 向已有面试会话提交候选人回答
     * @param request 提交回答请求
     * @return 网络层统一响应
     */
    InterviewNetResponse SubmitAnswer(const SubmitAnswerRequest& request);

    /**
     * @brief 为已有面试会话生成 JSON 报告
     * @param request 生成报告请求
     * @return 网络层统一响应，成功时 report_path 为写入路径
     */
    InterviewNetResponse GenerateReport(const GenerateReportRequest& request);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace net
} // namespace interview