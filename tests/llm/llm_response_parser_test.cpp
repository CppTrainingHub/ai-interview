#include "llm/llm_response_parser.h"

#include <cassert>
#include <stdexcept>
#include <string>

using interview::llm::ParseEvaluation;
using interview::llm::ParseQuestions;
using interview::llm::ParseSummaryText;

namespace {

void TestParseQuestionsFromMarkdownJson() {
    const std::string response = R"(下面是题目：
```json
[
  {
    "question": "什么是 RAII？",
    "category": "memory",
    "level": "basic",
    "key_points": ["resource management"],
    "expected_keywords": ["constructor", "destructor"]
  }
]
```
)";

    const auto questions = ParseQuestions(response);
    assert(questions.size() == 1);
    assert(questions[0].question == "什么是 RAII？");
    assert(questions[0].category == "memory");
    assert(questions[0].level == "basic");
    assert(questions[0].key_points.size() == 1);
    assert(questions[0].expected_keywords.size() == 2);
}

void TestParseEvaluationClampsScoreAndDisablesEmptyFollowup() {
    const std::string response = R"({
  "score": 120,
  "feedback": "回答不错",
  "need_followup": true,
  "followup_question": ""
})";

    const auto evaluation = ParseEvaluation(response);
    assert(evaluation.score == 100);
    assert(evaluation.feedback == "回答不错");
    assert(!evaluation.need_followup);
    assert(evaluation.followup_question.empty());
}

void TestParseSummaryJsonAndPlainText() {
    const auto summary = ParseSummaryText(R"({
  "summary": "整体表现良好",
  "recommendation": "推荐",
  "hiring_level": "中级 C++ 工程师"
})");

    assert(summary.find("整体表现良好") != std::string::npos);
    assert(summary.find("录用建议：推荐") != std::string::npos);
    assert(summary.find("建议级别：中级 C++ 工程师") != std::string::npos);

    const std::string plain = "这是一段纯文本总结";
    assert(ParseSummaryText(plain) == plain);
}

void TestInvalidQuestionsThrow() {
    bool threw = false;
    try {
        ParseQuestions("not json");
    } catch (const std::runtime_error&) {
        threw = true;
    }

    assert(threw);
}

} // namespace

int main() {
    TestParseQuestionsFromMarkdownJson();
    TestParseEvaluationClampsScoreAndDisablesEmptyFollowup();
    TestParseSummaryJsonAndPlainText();
    TestInvalidQuestionsThrow();
    return 0;
}
