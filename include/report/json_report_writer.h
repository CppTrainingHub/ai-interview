#pragma once

#include <memory>
#include <string>

#include "core/interview_ports.h"

namespace interview {
namespace report {

/**
 * @brief JSON 报告写入器
 * @note 负责将InterviewReport 写入 JSON 文件
 *       之前实现的ReportBuilder 负责生成结构化的报告数据， JsonReportWriter 只负责序列化和文件输出
 *       这个类实现 core::ReportWriter 接口,避免核心流程直接依赖 ofstream 或者 JSON 库
 */
class JsonReportWriter final : public core::ReportWriter {
public:
    /**
     * @brief 构造函数
     */
    JsonReportWriter();

    /**
     * @brief 析构函数
     */
    ~JsonReportWriter() override;

    JsonReportWriter(const JsonReportWriter&) = delete;
    JsonReportWriter& operator=(const JsonReportWriter&) = delete;

    JsonReportWriter(JsonReportWriter&&) noexcept;
    JsonReportWriter& operator=(JsonReportWriter&&) noexcept;

    /**
     * @brief 将结构化报告写入 JSON 文件
     * @param report ReportBuilder 或其他核心逻辑生成的结构化报告数据
     * @param output_path 输出文件路径，为空时默认文件名
     * @return 实际写入的 JSON 文件路径
     * @throws std::runtime_error 当文件无法打开或者写入失败时跑出异常
     */
    std::string WriteReport(const core::InterviewReport& report,
                             const std::string& output_path) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace report
} // namespace interview