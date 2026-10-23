#include "llm/llm_question_generator.h"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

using interview::llm::LLMClient;
using interview::llm::LLMMessage;
using interview::llm::LLMQuestionGenerator;

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

void TestGenerateFromResume() {
    FakeLLMClient client(R"([
  {
    "question": "介绍一下你的项目中如何使用多线程？",
    "category": "thread",
    "level": "intermediate",
    "key_points": ["thread"],
    "expected_keywords": ["mutex", "condition_variable"]
  }
])");

    LLMQuestionGenerator generator(client);
    const auto questions = generator.GenerateFromResume("C++ resume", 1);

    assert(questions.size() == 1);
    assert(questions[0].category == "thread");
    assert(client.last_message.find("C++ resume") != std::string::npos);
    assert(client.last_system_prompt.find("至少 1") != std::string::npos);
}

void TestGenerateDefault() {
    FakeLLMClient client(R"([
  {
    "question": "什么是 RAII？",
    "category": "memory",
    "level": "basic"
  }
])");

    LLMQuestionGenerator generator(client);
    const auto questions = generator.GenerateDefault(1);

    assert(questions.size() == 1);
    assert(questions[0].question == "什么是 RAII？");
    assert(client.last_message.find("C++开发工程师") != std::string::npos);
}

} // namespace

int main() {
    TestGenerateFromResume();
    TestGenerateDefault();
    return 0;
}
