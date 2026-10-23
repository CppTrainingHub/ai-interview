#pragma once

#include <ctime>
#include <memory>
#include <string>

#include "core/interview_models.h"
#include "core/interview_session.h"

namespace interview {
namespace core {

/**
 * @brief 面试报告构建器
 * ReportBuilder 只负责把 InterviewSession 中的状态整理成 InterviewReport
 * 这个模块不会完成内容：
 * - 不负责 JSON 序列化
 * - 不负责写文件
 * - 不依赖 nlohmann/json、ofstream、Qt 或平台 API
 * - 真正写文件的逻辑交给 ReportWriter
 */
class ReportBuilder {
public:
    /**
     * @brief 构造报告构建器
     */
    ReportBuilder();

    ~ReportBuilder();

    ReportBuilder(const ReportBuilder&) = delete;
    ReportBuilder& operator=(const ReportBuilder&) = delete;

    ReportBuilder(ReportBuilder&&) noexcept;
    ReportBuilder& operator=(ReportBuilder&&) noexcept;

    /**
     * @brief 根据会话状态生成结构化报告
     * @param session 面试会话对象，提供候选人、记录、分数、时长等信息
     * @param summary_text 面试总结文本，通常由 SummaryGenerator 生成
     * @param now 当前时间戳；由外部传入便于单元测试固定时间
     * @return InterviewReport 结构化报告数据
     */
    InterviewReport BuildReport(const InterviewSession& session,
                                std::string summary_text,
                                std::time_t now) const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace core
} // namespace interview
