#include "core/interview_session.h"
#include "core/report_builder.h"

#include <cassert>
#include <ctime>
#include <stdexcept>
#include <string>
#include <vector>

using interview::core::AnswerEvaluation;
using interview::core::InterviewQuestion;
using interview::core::InterviewSession;
using interview::core::InterviewStage;
using interview::core::ReportBuilder;

namespace {

std::vector<InterviewQuestion> MakeQuestions() {
    InterviewQuestion first;
    first.question = "什么是 RAII？";
    first.category = "memory";
    first.level = "basic";
    first.key_points = {"resource management"};
    first.expected_keywords = {"constructor", "destructor"};

    InterviewQuestion second;
    second.question = "std::vector 扩容时会发生什么？";
    second.category = "stl";
    second.level = "intermediate";
    second.key_points = {"capacity", "reallocation"};
    second.expected_keywords = {"capacity", "iterator"};

    return {first, second};
}

void TestQuestionFlowAndRecords() {
    InterviewSession session("Alice");

    assert(!session.HasQuestions());
    assert(session.GetCandidateName() == "Alice");
    assert(session.GetStage() == InterviewStage::kNotStarted);

    session.SetResumeText("C++ developer resume");
    session.SetQuestions(MakeQuestions());

    assert(session.HasQuestions());
    assert(session.GetResumeText() == "C++ developer resume");

    const std::string first = session.GetFirstQuestion();
    assert(first.find("第1题") != std::string::npos);
    assert(first.find("RAII") != std::string::npos);
    assert(session.GetStage() == InterviewStage::kQuestioning);
    assert(session.GetCurrentQuestionIndex() == 0);

    AnswerEvaluation eval;
    eval.score = 72;
    eval.feedback = "回答基本正确，但细节不足";
    eval.need_followup = true;
    eval.followup_question = "RAII 和异常安全有什么关系？";

    session.RecordAnswer("RAII 通过对象生命周期管理资源", eval, false);

    assert(session.GetLastScore() == 72);
    assert(session.GetTotalScore() == 72);
    assert(session.GetRecords().size() == 1);
    assert(session.ShouldFollowUp());

    const std::string followup = session.GetFollowUpQuestion();
    assert(followup.find("追问") != std::string::npos);
    assert(!session.ShouldFollowUp());
    assert(session.GetStage() == InterviewStage::kFollowUp);

    AnswerEvaluation followup_eval;
    followup_eval.score = 80;
    followup_eval.feedback = "追问回答较好";

    session.RecordAnswer("析构函数自动释放资源", followup_eval, true);
    assert(session.GetTotalScore() == 152);
    assert(session.GetRecords().size() == 2);

    const std::string second = session.GetNextQuestion();
    assert(second.find("第2题") != std::string::npos);
    assert(second.find("vector") != std::string::npos);
    assert(session.GetCurrentQuestionIndex() == 1);

    AnswerEvaluation second_eval;
    second_eval.score = 90;
    second_eval.feedback = "回答很好";

    session.RecordAnswer("可能重新分配内存，迭代器失效", second_eval, false);
    assert(session.GetTotalScore() == 242);

    const std::string done = session.GetNextQuestion();
    assert(done.empty());
    assert(session.isComplete());
    assert(session.GetStage() == InterviewStage::kCompleted);
}

void TestReportBuilder() {
    InterviewSession session("Bob");
    session.SetQuestions(MakeQuestions());
    session.GetFirstQuestion();

    AnswerEvaluation eval;
    eval.score = 85;
    eval.feedback = "回答准确";
    session.RecordAnswer("RAII uses constructors and destructors", eval, false);

    ReportBuilder builder;
    const std::time_t now = std::time(nullptr);
    auto report = builder.BuildReport(session, "总体表现良好", now);

    assert(report.candidate_name == "Bob");
    assert(report.total_question == 1);
    assert(report.total_score == 85);
    assert(report.average_score == 85.0);
    assert(report.evaluation.find("优秀") != std::string::npos);
    assert(report.summary_text == "总体表现良好");
    assert(report.records.size() == 1);
}

void TestThrowsWithoutQuestions() {
    InterviewSession session;

    bool threw = false;
    try {
        session.GetFirstQuestion();
    } catch (const std::runtime_error&) {
        threw = true;
    }

    assert(threw);
}

} // namespace

int main() {
    TestQuestionFlowAndRecords();
    TestReportBuilder();
    TestThrowsWithoutQuestions();
    return 0;
}
