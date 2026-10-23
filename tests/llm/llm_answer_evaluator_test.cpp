#include "llm/llm_answer_evaluator.h"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using interview::core::InterviewQuestion;
using interview::llm::LLMAnswerEvaluator;
using interview::llm::LLMClient;
using interview::llm::LLMMessage;

namespace {

class FakeLLMClient final : public LLMClient {
public:
    explicit FakeLLMClient(std::string response)
        : response_(std::move(response)) {}

    std::string SendMessage(const std::string& message,
                            const std::string& system_prompt) override {
        last_message = message;
        last_system_prompt = system_prompt;
        if (should_throw) {
            throw std::runtime_error("fake failure");
        }
        return response_;
    }

    std::string SendConversation(const std::vector<LLMMessage>&) override {
        return response_;
    }

    bool should_throw = false;
    std::string last_message;
    std::string last_system_prompt;

private:
    std::string response_;
};

InterviewQuestion MakeQuestion() {
    InterviewQuestion question;
    question.question = "什么是 shared_ptr？";
    question.category = "memory";
    question.level = "intermediate";
    question.key_points = {"reference count"};
    question.expected_keywords = {"shared_ptr", "weak_ptr"};
    return question;
}

void TestEvaluateNormalAnswer() {
    FakeLLMClient client(R"({
  "score": 78,
  "feedback": "基本正确",
  "need_followup": true,
  "followup_question": "引用计数如何维护？"
})");

    LLMAnswerEvaluator evaluator(client);
    const auto evaluation = evaluator.Evaluate(MakeQuestion(), "shared_ptr 使用引用计数", false);

    assert(evaluation.score == 78);
    assert(evaluation.need_followup);
    assert(evaluation.followup_question == "引用计数如何维护？");
    assert(client.last_message.find("shared_ptr") != std::string::npos);
}

void TestFollowupAnswerDoesNotTriggerAnotherFollowup() {
    FakeLLMClient client(R"({
  "score": 80,
  "feedback": "追问回答不错",
  "need_followup": true,
  "followup_question": "继续追问"
})");

    LLMAnswerEvaluator evaluator(client);
    const auto evaluation = evaluator.Evaluate(MakeQuestion(), "追问回答", true);

    assert(evaluation.score == 80);
    assert(!evaluation.need_followup);
    assert(evaluation.followup_question.empty());
}

void TestFallbackOnFailure() {
    FakeLLMClient client("{}");
    client.should_throw = true;

    LLMAnswerEvaluator evaluator(client);
    const auto evaluation = evaluator.Evaluate(MakeQuestion(), "answer", false);

    assert(evaluation.score == 50);
    assert(!evaluation.need_followup);
    assert(evaluation.feedback.find("默认中等分数") != std::string::npos);
}

} // namespace

int main() {
    TestEvaluateNormalAnswer();
    TestFollowupAnswerDoesNotTriggerAnotherFollowup();
    TestFallbackOnFailure();
    return 0;
}
