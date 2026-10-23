#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

#include "core/report_builder.h"

namespace interview {
namespace core {
namespace {

/**
 * @brief 跨平台安全获取本地时间
 * @param time 原始 time_t 时间戳
 * @return std::tm 本地时间结构
 */
std::tm SafeLocalTime(std::time_t time) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    return tm;
}

/**
 * @brief 格式化报告时间
 * @param time 时间戳
 * @return 格式化后的时间字符串，例如 2026-06-04 10:30:00
 */
std::string FormatDateTime(std::time_t time) {
    auto tm = SafeLocalTime(time);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/**
 * @brief 根据平均分生成综合评价
 * @param average_score 平均分
 * @return 中文综合评价文本
 */
std::string BuildEvaluation(double average_score) {
    if (average_score >= 85.0) {
        return "优秀 - C++技术功底深厚，强烈推荐";
    }

    if (average_score >= 70.0) {
        return "良好 - C++基础扎实，推荐录用";
    }

    if (average_score >= 60.0) {
        return "中等 - 基础尚可，需继续学习";
    }

    return "待提高 - 基础薄弱，建议加强学习";
}

/**
 * @brief 保留一位小数
 * @param value 原始数值
 * @return 四舍五入到一位小数后的数值
 */
double RoundOneDecimal(double value) {
    return std::round(value * 10.0) / 10.0;
}

} // namespace

class ReportBuilder::Impl {
public:
    /**
     * @brief 构建结构化报告
     * @param session 面试会话
     * @param summary_text 总结文本
     * @param now 当前时间戳
     * @return InterviewReport
     */
    InterviewReport BuildReport(const InterviewSession& session,
                                std::string summary_text,
                                std::time_t now) const {
        // 创建报告结构对象
        InterviewReport report;

        // 填充
        report.candidate_name = session.GetCandidateName();
        report.interview_date = FormatDateTime(now);
        report.duration_minutes = RoundOneDecimal(session.GetDurationMinutes(now));

        report.total_question = static_cast<int>(session.GetRecords().size());
        report.total_score = session.GetTotalScore();

        if (report.total_question > 0) {
            report.average_score =
                RoundOneDecimal(static_cast<double>(report.total_score) / report.total_question);
        }

        report.evaluation = BuildEvaluation(report.average_score);

        report.summary_text = std::move(summary_text);
        report.records = session.GetRecords();

        return report;
    }
};

ReportBuilder::ReportBuilder()
    : pImpl(std::make_unique<Impl>()) {}

ReportBuilder::~ReportBuilder() = default;

ReportBuilder::ReportBuilder(ReportBuilder&&) noexcept = default;

ReportBuilder& ReportBuilder::operator=(ReportBuilder&&) noexcept = default;

InterviewReport ReportBuilder::BuildReport(const InterviewSession& session,
                                           std::string summary_text,
                                           std::time_t now) const {
    return pImpl->BuildReport(session, std::move(summary_text), now);
}

} // namespace core
} // namespace interview
