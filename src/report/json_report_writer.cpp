#include <cstddef>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "report/json_report_writer.h"

namespace interview {
namespace report {
namespace {

/**
 * @brief 转义 JSON 字符串中的特殊字符
 * @param value 原始字符串
 * @return 可安全写入 JSON 字符串值的内容
 */
std::string EscapeJsonString(const std::string& value) {
    std::ostringstream oss;

    for (char ch : value) {
        switch (ch) {
            case '\\':
                oss << "\\\\";
                break;
            case '"':
                oss << "\\\"";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                oss << ch;
                break;
        }
    }

    return oss.str();
}

/**
 * @brief 给字符串添加 JSON 引号
 * @param value 原始字符串
 * @return 带引号且已转义的 JSON 字符
 */
std::string Quote(const std::string& value) {
    return "\"" + EscapeJsonString(value) + "\"";
}

/**
 * @brief 生成默认报告输出路径
 * @param report 结构化面试报告
 * @return 默认Json文件名
 */
std::string MakeDefaultOutputPath(const core::InterviewReport& report) {
    // 首先确认候选人姓名
    std::string name = report.candidate_name.empty()
                     ? "candidate"
                     : report.candidate_name;
    
    // 将空格或者其他命名方式，转换成下划线_
    for (char& ch : name) {
        if (ch == ' ' || ch == '/' || ch == '\\' || ch == ':') {
            ch = '_';
        }
    }

    // 生成对应的报告名称
    return name + "_interview_report.json";
}

/**
 * @brief 写入字符串数组
 * @param os 输出流
 * @param values 字符串数组
 */
void WriteStringArray(std::ostream& os, const std::vector<std::string>& values) {
    os << "[";

    for (std::size_t i = 0; i < values.size(); i++) {
        if (i > 0) {
            os << ", ";
        }

        os << Quote(values[i]);
    }

    os << "]";
}

/**
 * @brief 写入面试题 JSON 对象
 * @param os 输出流
 * @param question 面试题结构
 */
void WriteQuestion(std::ostream& os, const core::InterviewQuestion& question) {
    os << "{";
    os << "\"question\": " << Quote(question.question) << ", ";
    os << "\"category\": " << Quote(question.category) << ", ";
    os << "\"level\": " << Quote(question.level) << ", ";
    os << "\"key_points\": ";
    WriteStringArray(os, question.key_points);
    os << ", ";
    os << "\"expected_keywords\": ";
    WriteStringArray(os, question.expected_keywords);
    os << "}";
}

/**
 * @brief 写入回答评分 JSON 对象
 * @param os 输出流
 * @param evaluation 回答评分结果
 */
void WriteEvaluation(std::ostream& os, const core::AnswerEvaluation& evaluation) {
    os << "{";
    os << "\"score\": " << evaluation.score << ", ";
    os << "\"feedback\": " << Quote(evaluation.feedback) << ", ";
    os << "\"need_followup\": "
       << (evaluation.need_followup ? "true" : "false") << ", ";
    os << "\"followup_question\": " << Quote(evaluation.followup_question);
    os << "}";
}

/**
 * @brief 写入单条问答记录 JSON 对象
 * @param os 输出流
 * @param record 面试问答记录
 */
void WriteRecord(std::ostream& os, const core::InterviewRecord& record) {
    os << "{";
    os << "\"question\": ";
    WriteQuestion(os, record.question);
    os << ", ";
    os << "\"answer\": " << Quote(record.answer) << ", ";
    os << "\"timestamp\": " << Quote(record.timestamp) << ", ";
    os << "\"evaluation\": ";
    WriteEvaluation(os, record.evaluation);
    os << ", ";
    os << "\"is_followup\": "
       << (record.is_followup ? "true" : "false");
    os << "}";
}
} // namespace

class JsonReportWriter::Impl {
public:
    /**
     * @brief 将结构化报告写入 JSON 文件
     * @param report 结构化报告数据
     * @param output_path 输出路径；为空时生成默认路径
     * @return 实际写入的文件路径
     */
    std::string WriteReport(const core::InterviewReport& report,
                            const std::string& output_path) {

        // 明确输出路径
        const std::string actual_path = output_path.empty() ? MakeDefaultOutputPath(report) : output_path;

        std::ofstream file(actual_path);
        if (!file) {
            throw std::runtime_error("failed to open output file: " + actual_path);
        }

        // 将结构化报告序列化为JSON字符串
        file << SerializeReport(report);

        if (!file) {
            throw std::runtime_error("failed to write report output file: " + actual_path);
        }

        return actual_path;
    }


private:
    /**
     * @brief 将 InterviewReport 序列化为 JSON 字符串
     * @param report 结构化面试报告
     * @return JSON 字符串
     */
    static std::string SerializeReport(const core::InterviewReport& report) {
        std::ostringstream os;

        // 将对应的内容写入输出流
        os << "{\n";
        os << "  \"candidate_name\": " << Quote(report.candidate_name) << ",\n";
        os << "  \"interview_date\": " << Quote(report.interview_date) << ",\n";
        os << "  \"duration_minutes\": " << report.duration_minutes << ",\n";
        os << "  \"total_question\": " << report.total_question << ",\n";
        os << "  \"total_score\": " << report.total_score << ",\n";
        os << "  \"average_score\": " << report.average_score << ",\n";
        os << "  \"evaluation\": " << Quote(report.evaluation) << ",\n";
        os << "  \"summary_text\": " << Quote(report.summary_text) << ",\n";
        os << "  \"records\": [\n";

        for (std::size_t i = 0; i < report.records.size(); i++) {
            os << "  ";
            WriteRecord(os, report.records[i]);

            if (i + 1 < report.records.size()) {
                os << ",";
            }
            
            os << "\n";
        }

        os << "  ]\n";
        os << "}\n";

        return os.str();
    }
};

JsonReportWriter::JsonReportWriter() : pImpl(std::make_unique<Impl>()){}

JsonReportWriter::~JsonReportWriter() = default;

JsonReportWriter::JsonReportWriter(JsonReportWriter&&) noexcept = default;
JsonReportWriter& JsonReportWriter::operator=(JsonReportWriter&&) noexcept = default;

std::string JsonReportWriter::WriteReport(const core::InterviewReport& report,
                                          const std::string& output_path) {
    return pImpl->WriteReport(report, output_path);  
}


} // namespace report
} // namespace interview
