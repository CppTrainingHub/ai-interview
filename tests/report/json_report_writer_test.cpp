#include "report/json_report_writer.h"

#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

using interview::core::AnswerEvaluation;
using interview::core::InterviewQuestion;
using interview::core::InterviewRecord;
using interview::core::InterviewReport;
using interview::report::JsonReportWriter;

namespace {

/**
 * @brief 读取文本文件内容
 * @param path 文件路径
 * @return 文件完整文本内容
 */
std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    assert(file);

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

/**
 * @brief 构造测试用报告对象
 * @return 填充完整字段的 InterviewReport
 */
InterviewReport MakeReport() {
    InterviewQuestion question;
    question.question = "什么是 RAII？";
    question.category = "memory";
    question.level = "basic";
    question.key_points = {"resource management"};
    question.expected_keywords = {"constructor", "destructor"};

    AnswerEvaluation evaluation;
    evaluation.score = 88;
    evaluation.feedback = "回答准确，表达清晰";
    evaluation.need_followup = false;

    InterviewRecord record;
    record.question = question;
    record.answer = "RAII uses constructors and destructors";
    record.timestamp = "10:30:00";
    record.evaluation = evaluation;
    record.is_followup = false;

    InterviewReport report;
    report.candidate_name = "Alice";
    report.interview_date = "2026-06-06 10:30:00";
    report.duration_minutes = 12.5;
    report.total_question = 1;
    report.total_score = 88;
    report.average_score = 88.0;
    report.evaluation = "优秀 - C++技术功底深厚，强烈推荐";
    report.summary_text = "整体表现优秀";
    report.records.push_back(record);

    return report;
}

/**
 * @brief 测试显式路径写入 JSON 报告
 */
void TestWriteReportToExplicitPath() {
    const std::string path = "/tmp/ai_interview_json_report_test.json";

    JsonReportWriter writer;
    const auto written_path = writer.WriteReport(MakeReport(), path);

    assert(written_path == path);

    const auto content = ReadFile(path);
    assert(content.find("\"candidate_name\": \"Alice\"") != std::string::npos);
    assert(content.find("\"summary_text\": \"整体表现优秀\"") != std::string::npos);
    assert(content.find("\"total_score\": 88") != std::string::npos);
    assert(content.find("\"records\"") != std::string::npos);
    assert(content.find("\"question\": \"什么是 RAII？\"") != std::string::npos);
    assert(content.find("\"score\": 88") != std::string::npos);
}

/**
 * @brief 测试 JSON 字符串转义
 */
void TestEscapesJsonStrings() {
    InterviewReport report = MakeReport();
    report.candidate_name = "Alice \"Cpp\"";
    report.summary_text = "line1\nline2";

    const std::string path = "/tmp/ai_interview_json_escape_test.json";

    JsonReportWriter writer;
    writer.WriteReport(report, path);

    const auto content = ReadFile(path);
    assert(content.find("Alice \\\"Cpp\\\"") != std::string::npos);
    assert(content.find("line1\\nline2") != std::string::npos);
}

/**
 * @brief 测试空路径时生成默认输出路径
 */
void TestDefaultOutputPath() {
    InterviewReport report = MakeReport();
    report.candidate_name = "Bob Smith";

    JsonReportWriter writer;
    const auto written_path = writer.WriteReport(report, "");

    assert(written_path == "Bob_Smith_interview_report.json");

    const auto content = ReadFile(written_path);
    assert(content.find("\"candidate_name\": \"Bob Smith\"") != std::string::npos);
}

} // namespace

int main() {
    TestWriteReportToExplicitPath();
    TestEscapesJsonStrings();
    TestDefaultOutputPath();
    return 0;
}