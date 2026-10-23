#include <sstream>
#include <string>
#include <utility>

#include "llm/llm_summary_generator.h"
#include "llm/llm_response_parser.h"

namespace interview {
namespace llm {
namespace {

/**
 * @brief 构造总结生成系统提示词
 * @return 系统提示词
 */
std::string BuildSummarySystemPrompt() {
    return "你是一位资深 C++ 技术面试官。"
           "请根据面试记录生成中文总结，包含整体评价、优势、不足和录用建议。"
           "可以返回 JSON 对象，字段包括 summary、recommendation、hiring_level。";
}

/**
 * @brief 将单条面试记录写入文本流
 * @param oss 输出文本流
 * @param record 面试记录
 */
void AppendRecordText(std::ostringstream& oss, const core::InterviewRecord& record) {
    oss << "问题：" << record.question.question << "\n";
    oss << "类别：" << record.question.category << "\n";
    oss << "难度：" << record.question.level << "\n";
    oss << "回答：" << record.answer << "\n";
    oss << "分数：" << record.evaluation.score << "\n";
    oss << "反馈：" << record.evaluation.feedback << "\n";
    oss << "是否追问：" << (record.is_followup ? "是" : "否") << "\n\n";
}

/**
 * @brief 构造总结生成的用户输入
 * @param records 面试记录列表
 * @param resume_text 简历文本，可为空
 * @return 可发送给 LLM 的总结输入文本
 */
std::string BuildSummaryUserPrompt(const std::vector<core::InterviewRecord>& records,
                                   const std::string& resume_text) {
    std::ostringstream oss;

    if (!resume_text.empty()) {
        oss << "候选人简历：\n" << resume_text << "\n\n";
    }

    oss << "面试记录：\n";
    for (const auto& record : records) {
        AppendRecordText(oss, record);
    }

    return oss.str();
}

} // namespace

class LLMSummaryGenerator::Impl {
public:
    explicit Impl(LLMClient& client_ref) : client(client_ref) {}

    /**
     * @brief 根据面试记录生成总结
     * @param records 面试记录列表
     * @param resume_text 简历文本，可为空
     * @return 面试总结文本
     */
    std::string GenerateSummary(const std::vector<core::InterviewRecord>& records,
                                const std::string& resume_text) {
        if (records.empty()) {
            return "本次面试暂无回答记录，无法生成完整总结。";
        }

        const auto& response = client.SendMessage(
            BuildSummaryUserPrompt(records, resume_text),
            BuildSummarySystemPrompt()
        );

        return ParseSummaryText(response);
    }
private:
    LLMClient& client;
};

LLMSummaryGenerator::LLMSummaryGenerator(LLMClient& client)
    : pImpl(std::make_unique<Impl>(client)) {}

LLMSummaryGenerator::~LLMSummaryGenerator() = default;
LLMSummaryGenerator::LLMSummaryGenerator(LLMSummaryGenerator&&) noexcept = default;
LLMSummaryGenerator& LLMSummaryGenerator::operator=(LLMSummaryGenerator&&) noexcept = default;

std::string LLMSummaryGenerator::GenerateSummary(
    const std::vector<core::InterviewRecord>& records,
    const std::string& resume_text) {
    return pImpl->GenerateSummary(records, resume_text);
}


} // namespace llm
} // namespace interview