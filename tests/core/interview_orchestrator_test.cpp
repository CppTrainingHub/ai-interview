#include "core/interview_orchestrator.h"

#include <cassert>
#include <string>
#include <vector>

using interview::core::AnswerEvaluation;
using interview::core::AnswerEvaluator;
using interview::core::InterviewActionType;
using interview::core::InterviewOrchestrator;
using interview::core::InterviewQuestion;
using interview::core::InterviewRecord;
using interview::core::InterviewSession;
using interview::core::QuestionGenerator;
using interview::core::ResumeParser;
using interview::core::SummaryGenerator;

namespace {

class MockResumeParser : public ResumeParser {
public:
    std::string ExtractText(const std::string& resume_path) override {
        last_path = resume_path;
        return "mock resume text";
    }

    std::string last_path;
};

class MockQuestionGenerator : public QuestionGenerator {
public:
    std::vector<InterviewQuestion> GenerateFromResume(
        const std::string& resume_text,
        int min_questions) override {
        last_resume_text = resume_text;
        last_min_questions = min_questions;
        return MakeQuestions();
    }

    std::vector<InterviewQuestion> GenerateDefault(int min_questions) override {
        last_min_questions = min_questions;
        return MakeQuestions();
    }

    static std::vector<InterviewQuestion> MakeQuestions() {
        InterviewQuestion first;
        first.question = "什么是虚函数？";
        first.category = "oop";
        first.level = "basic";

        InterviewQuestion second;
        second.question = "如何理解 move 语义？";
        second.category = "modern_cpp";
        second.level = "intermediate";

        return {first, second};
    }

    std::string last_resume_text;
    int last_min_questions = 0;
};

class MockAnswerEvaluator : public AnswerEvaluator {
public:
    AnswerEvaluation Evaluate(
        const InterviewQuestion& question,
        const std::string& answer,
        bool is_followup) override {
        last_question = question.question;
        last_answer = answer;
        last_is_followup = is_followup;
        ++call_count;

        AnswerEvaluation eval;
        eval.score = is_followup ? 80 : 70;
        eval.feedback = "mock feedback";

        if (!is_followup && should_followup) {
            eval.need_followup = true;
            eval.followup_question = "请进一步解释底层机制。";
        }

        return eval;
    }

    bool should_followup = false;
    int call_count = 0;
    bool last_is_followup = false;
    std::string last_question;
    std::string last_answer;
};

class MockSummaryGenerator : public SummaryGenerator {
public:
    std::string GenerateSummary(
        const std::vector<InterviewRecord>& records,
        const std::string& resume_text) override {
        record_count = records.size();
        last_resume_text = resume_text;
        return "mock summary";
    }

    std::size_t record_count = 0;
    std::string last_resume_text;
};

void TestDefaultInterviewFlow() {
    InterviewSession session("Candidate");
    MockResumeParser parser;
    MockQuestionGenerator question_generator;
    MockAnswerEvaluator evaluator;
    MockSummaryGenerator summary_generator;

    InterviewOrchestrator orchestrator(
        session,
        parser,
        question_generator,
        evaluator,
        summary_generator);

    orchestrator.ConfigureDefaultInterview(2);

    auto intro = orchestrator.Start();
    assert(intro.type == InterviewActionType::kSpeakIntro);
    assert(!intro.text.empty());

    auto first_question = orchestrator.ProcessCandidateAnswer("我是候选人");
    assert(first_question.type == InterviewActionType::kAskQuestion);
    assert(first_question.text.find("第1题") != std::string::npos);
    assert(evaluator.call_count == 0);

    auto second_question = orchestrator.ProcessCandidateAnswer("虚函数支持动态绑定");
    assert(second_question.type == InterviewActionType::kAskQuestion);
    assert(second_question.text.find("第2题") != std::string::npos);
    assert(evaluator.call_count == 1);
    assert(!evaluator.last_is_followup);

    auto finish = orchestrator.ProcessCandidateAnswer("move 可以转移资源");
    assert(finish.type == InterviewActionType::kFinish);
    assert(finish.text == "mock summary");
    assert(evaluator.call_count == 2);
    assert(summary_generator.record_count == 2);
}

void TestFollowUpFlow() {
    InterviewSession session("Candidate");
    MockResumeParser parser;
    MockQuestionGenerator question_generator;
    MockAnswerEvaluator evaluator;
    MockSummaryGenerator summary_generator;

    evaluator.should_followup = true;

    InterviewOrchestrator orchestrator(
        session,
        parser,
        question_generator,
        evaluator,
        summary_generator);

    orchestrator.ConfigureDefaultInterview(2);
    orchestrator.Start();
    orchestrator.ProcessCandidateAnswer("自我介绍");

    auto followup = orchestrator.ProcessCandidateAnswer("回答不够深入");
    assert(followup.type == InterviewActionType::kAskFollowUp);
    assert(followup.text.find("追问") != std::string::npos);
    assert(evaluator.call_count == 1);

    auto next_question = orchestrator.ProcessCandidateAnswer("追问回答");
    assert(next_question.type == InterviewActionType::kAskQuestion);
    assert(evaluator.call_count == 2);
    assert(evaluator.last_is_followup);
}

void TestResumeInterviewConfiguration() {
    InterviewSession session("Candidate");
    MockResumeParser parser;
    MockQuestionGenerator question_generator;
    MockAnswerEvaluator evaluator;
    MockSummaryGenerator summary_generator;

    InterviewOrchestrator orchestrator(
        session,
        parser,
        question_generator,
        evaluator,
        summary_generator);

    orchestrator.ConfigureResumeInterview("resume.pdf", 5);

    assert(parser.last_path == "resume.pdf");
    assert(question_generator.last_resume_text == "mock resume text");
    assert(question_generator.last_min_questions == 5);
    assert(session.GetResumeText() == "mock resume text");
    assert(session.HasQuestions());
}

} // namespace

int main() {
    TestDefaultInterviewFlow();
    TestFollowUpFlow();
    TestResumeInterviewConfiguration();
    return 0;
}
