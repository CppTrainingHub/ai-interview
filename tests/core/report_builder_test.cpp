#include "core/interview_session.h"
#include "core/report_builder.h"

#include <cassert>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>

using interview::core::AnswerEvaluation;
using interview::core::InterviewQuestion;
using interview::core::InterviewReport;
using interview::core::InterviewSession;
using interview::core::ReportBuilder;

namespace {

std::vector<InterviewQuestion> MakeQuestions() {
    InterviewQuestion first;
    first.question = "什么是 RAII？";
    first.category = "memory";
    first.level = "basic";

    InterviewQuestion second;
    second.question = "std::vector 扩容时会发生什么？";
    second.category = "stl";
    second.level = "intermediate";

    return {first, second};
}

void RecordCurrentAnswer(InterviewSession& session,
                         const std::string& answer,
                         int score) {
    AnswerEvaluation evaluation;
    evaluation.score = score;
    evaluation.feedback = "test feedback";
    session.RecordAnswer(answer, evaluation, false);
}

InterviewReport BuildReportWithScores(const std::vector<int>& scores,
                                      const std::string& summary_text) {
    InterviewSession session("ScoreCandidate");
    session.SetQuestions(MakeQuestions());
    session.GetFirstQuestion();

    for (std::size_t i = 0; i < scores.size(); ++i) {
        RecordCurrentAnswer(session, "answer", scores[i]);
        if (i + 1 < scores.size()) {
            session.GetNextQuestion();
        }
    }

    ReportBuilder builder;
    return builder.BuildReport(session, summary_text, std::time(nullptr));
}

void TestBuildReportCopiesSessionData() {
    InterviewSession session("Alice");
    session.SetResumeText("C++ developer resume");
    session.SetQuestions(MakeQuestions());
    session.GetFirstQuestion();

    RecordCurrentAnswer(session, "RAII 通过对象生命周期管理资源", 80);
    session.GetNextQuestion();
    RecordCurrentAnswer(session, "vector 扩容可能导致迭代器失效", 81);

    ReportBuilder builder;
    const std::time_t now = std::time(nullptr);
    const auto report = builder.BuildReport(session, "整体表现稳定", now);

    assert(report.candidate_name == "Alice");
    assert(report.total_question == 2);
    assert(report.total_score == 161);
    assert(std::fabs(report.average_score - 80.5) < 0.001);
    assert(report.evaluation.find("良好") != std::string::npos);
    assert(report.summary_text == "整体表现稳定");
    assert(report.records.size() == 2);
    assert(report.records[0].answer.find("RAII") != std::string::npos);
    assert(report.records[1].answer.find("vector") != std::string::npos);
    assert(report.duration_minutes >= 0.0);
    assert(report.interview_date.size() == 19);
    assert(report.interview_date[4] == '-');
    assert(report.interview_date[7] == '-');
    assert(report.interview_date[10] == ' ');
    assert(report.interview_date[13] == ':');
    assert(report.interview_date[16] == ':');
}

void TestEvaluationLevels() {
    auto excellent = BuildReportWithScores({85}, "excellent");
    assert(excellent.evaluation.find("优秀") != std::string::npos);

    auto good = BuildReportWithScores({70}, "good");
    assert(good.evaluation.find("良好") != std::string::npos);

    auto medium = BuildReportWithScores({60}, "medium");
    assert(medium.evaluation.find("中等") != std::string::npos);

    auto weak = BuildReportWithScores({59}, "weak");
    assert(weak.evaluation.find("待提高") != std::string::npos);
}

void TestEmptySessionReport() {
    InterviewSession session("EmptyCandidate");

    ReportBuilder builder;
    const auto report = builder.BuildReport(session, "暂无回答", std::time(nullptr));

    assert(report.candidate_name == "EmptyCandidate");
    assert(report.total_question == 0);
    assert(report.total_score == 0);
    assert(report.average_score == 0.0);
    assert(report.evaluation.find("待提高") != std::string::npos);
    assert(report.summary_text == "暂无回答");
    assert(report.records.empty());
}

} // namespace

int main() {
    TestBuildReportCopiesSessionData();
    TestEvaluationLevels();
    TestEmptySessionReport();
    return 0;
}
