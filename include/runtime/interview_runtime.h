#pragma once

#include <memory>
#include <functional>
#include <string>

#include "config/app_config.h"
#include "core/interview_orchestrator.h"

namespace interview {
namespace runtime {

/**
 * @brief 单次面试运行时
 * @note 该类只负责装配外部端口实现并驱动 core 编排器，不承载面试业务规则。
 */
class InterviewRuntime {
public:
    /**
     * @brief 创建面试运行时
     * @param config 应用配置，决定使用真实 LLM、离线 scripted 实现或 audio 链路
     * @param candidate_name 候选人姓名
     */
    InterviewRuntime(config::AppConfig config, std::string candidate_name);

    ~InterviewRuntime();

    InterviewRuntime(const InterviewRuntime&) = delete;
    InterviewRuntime& operator=(const InterviewRuntime&) = delete;

    InterviewRuntime(InterviewRuntime&&) noexcept;
    InterviewRuntime& operator=(InterviewRuntime&&) noexcept;

    /**
     * @brief 配置题目并启动面试
     * @param resume_path 简历路径；为空时使用默认题目
     * @param min_questions 最少题目数量
     * @return core 面试动作
     */
    core::InterviewAction Start(const std::string& resume_path, int min_questions);

    /**
     * @brief 提交一段文本回答并获取下一步动作
     * @param answer 候选人回答文本
     * @return core 面试动作
     */
    core::InterviewAction SubmitTextAnswer(const std::string& answer);

    /**
     * @brief 从音频链路采集回答文本并提交给面试流程
     * @return core 面试动作
     */
    core::InterviewAction CaptureAndSubmitAudioAnswer();

    /**
     * @brief 在后台开始采集候选人回答。
     * @param on_action ASR 文本提交后产生的下一步面试动作回调。
     * @param on_error 异步错误回调。
     * @note 回调运行于工作线程，GUI 必须转发到主线程。
     */
    void BeginAsyncAudioAnswer(
        std::function<void(const core::InterviewAction&)> on_action,
        std::function<void(const std::string&)> on_error = {},
        std::function<void(const std::string&)> on_transcript = {});

    /** @brief 非阻塞请求结束当前异步回答录音。 */
    void EndAsyncAudioAnswer();

    /**
     * @brief 在后台朗读面试动作。
     * @param action 要朗读的动作。
     * @param on_complete 播放完成回调。
     * @param on_error 异步错误回调。
     */
    void SpeakActionAsync(
        const core::InterviewAction& action,
        std::function<void()> on_complete = {},
        std::function<void(const std::string&)> on_error = {});

    /** @brief 等待当前异步语音操作结束。 */
    void WaitForAudioOperation();

    /**
     * @brief 朗读或播放一条面试动作文本
     * @param action 面试动作
     */
    void SpeakAction(const core::InterviewAction& action);

    /**
     * @brief 生成当前面试报告
     * @param output_suffix 输出文件名后缀，用于区分 CLI 或网络会话
     * @return 报告文件路径
     */
    std::string GenerateReport(const std::string& output_suffix);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace runtime
} // namespace interview
