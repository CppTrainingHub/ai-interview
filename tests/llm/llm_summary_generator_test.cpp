#include "llm/llm_summary_generator.h"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

using interview::core::AnswerEvaluation;
using interview::core::InterviewQuestion;
using interview::core::InterviewRecord;
using interview::llm::LLMClient;
using interview::llm::LLMMessage;
using interview::llm::LLMSummaryGenerator;

namespace {

class FakeLLMClient final : public LLMClient {
public:
    explicit FakeLLMClient(std::string response)
        : response_(std::move(response)) {}

    std::string SendMessage(const std::string& message,
                            const std::string& system_prompt) override {
        last_message = message;
        last_system_prompt = system_prompt;
        return response_;
    }

    std::string SendConversation(const std::vector<LLMMessage>&) override {
        return response_;
    }

    std::string last_message;
    std::string last_system_prompt;

private:
    std::string response_;
};

std::vector<InterviewRecord> MakeRecords() {
    InterviewQuestion question;
    question.question = "什么是 RAII？";
    question.category = "memory";
    question.level = "basic";

    AnswerEvaluation evaluation;
    evaluation.score = 85;
    evaluation.feedback = "回答准确";

    InterviewRecord record;
    record.question = question;
    record.answer = "对象生命周期管理资源";
    record.evaluation = evaluation;
    record.is_followup = false;

    return {record};
}

void TestGenerateSummary() {
    FakeLLMClient client(R"({
  "summary": "整体表现良好",
  "recommendation": "推荐",
  "hiring_level": "中级"
})");

    LLMSummaryGenerator generator(client);
    const auto summary = generator.GenerateSummary(MakeRecords(), "resume text");

    assert(summary.find("整体表现良好") != std::string::npos);
    assert(summary.find("录用建议：推荐") != std::string::npos);
    assert(client.last_message.find("resume text") != std::string::npos);
    assert(client.last_message.find("RAII") != std::string::npos);
}

void TestEmptyRecords() {
    FakeLLMClient client("should not be used");
    LLMSummaryGenerator generator(client);

    const auto summary = generator.GenerateSummary({}, "");
    assert(summary.find("暂无回答记录") != std::string::npos);
}

} // namespace

int main() {
    TestGenerateSummary();
    TestEmptyRecords();
    return 0;
}
