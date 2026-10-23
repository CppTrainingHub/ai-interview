#include "core/interview_orchestrator.h"

#include <stdexcept>
#include <utility>

namespace interview {
namespace core {

class InterviewOrchestrator::Impl {
public:
    /**
     * @brief 构造函数
     * @param session_ref 面试状态容器
     * @param resume_parser_ref 简历解析端口
     * @param question_generator_ref 题目生成端口
     * @param answer_evaluator_ref 回答评分端口
     * @param summary_generator_ref 总结生成端口
     */
    Impl(InterviewSession& session_ref,
         ResumeParser& resume_parser_ref,
         QuestionGenerator& question_generator_ref,
         AnswerEvaluator& answer_evaluator_ref,
         SummaryGenerator& summary_generator_ref)
        : session(session_ref),
          resume_parser(resume_parser_ref),
          question_generator(question_generator_ref),
          answer_evaluator(answer_evaluator_ref),
          summary_generator(summary_generator_ref) {}

    /**
     * @brief 配置简历驱动面试
     * @param resume_path 简历文件路径
     * @param min_questions 希望生成的最少题目数量
     */
    void ConfigureResumeInterview(const std::string& resume_path, int min_questions) {
        /**
            流程：
                1. 调用 ResumeParser 提取简历文本
                2. 调用 QuestionGenerator 根据简历生成题目
                3. 将简历文本和题目写入 InterviewSession
         */
        std::string resume_text = resume_parser.ExtractText(resume_path);

        auto questions = question_generator.GenerateFromResume(resume_text, min_questions);

        session.SetResumeText(std::move(resume_text));
        session.SetQuestions(std::move(questions));

        ResetFlowFlags();
    }

    /**
     * @brief 配置默认面试
     * @param min_questions 希望生成的最少题目数量
     * 没有简历时使用默认题目。题目来源可以是 LLM，也可以是规则题库
     */
    void ConfigureDefaultInterview(int min_questions) {
        auto questions = question_generator.GenerateDefault(min_questions);
        session.SetQuestions(std::move(questions));

        ResetFlowFlags();
    }

    /**
     * @brief 开始面试
     * @return kSpeakIntro 动作，text 为开场白提示词
     * @throws std::runtime_error 如果还没有配置题目
     */
    InterviewAction Start() {
        EnsureConfigured();
        ResetFlowFlags();

        return MakeAction(InterviewActionType::kSpeakIntro, session.GetIntroPrompt());
    }

    /**
     * @brief 处理候选人回答
     * @param answer 候选人的回答文本
     * @return 下一步动作：问第一题、追问、问下一题或结束
     */
    InterviewAction ProcessCandidateAnswer(const std::string& answer) {
        EnsureConfigured();

        // 第一段候选人输入是自我介绍，不评分，直接进入第一道技术题。
        if (!intro_answer_received) {
            intro_answer_received = true;

            return MakeAction(InterviewActionType::kAskQuestion, session.GetFirstQuestion());
        }

        const bool is_followup = waiting_for_followup_answer;

        // 技术题回答会调用 AnswerEvaluator 评分
        AnswerEvaluation evaluation = answer_evaluator.Evaluate(
            session.GetCurrentQuestion(),
            answer,
            is_followup);

        session.RecordAnswer(answer, evaluation, is_followup);

        // 普通题回答后，如果 evaluator 要求追问，则优先追问
        // 如果无需追问，则进入下一题
        if (!is_followup && session.ShouldFollowUp()) {
            waiting_for_followup_answer = true;

            return MakeAction(InterviewActionType::kAskFollowUp, session.GetFollowUpQuestion());
        }

        waiting_for_followup_answer = false;

        std::string next_question = session.GetNextQuestion();
        if (!next_question.empty()) {
            return MakeAction(InterviewActionType::kAskQuestion, std::move(next_question));
        }

        // 如果没有下一题，则生成总结并结束
        std::string summary = summary_generator.GenerateSummary(
            session.GetRecords(),
            session.GetResumeText());

        return MakeAction(InterviewActionType::kFinish, std::move(summary));
    }

private:
    /**
     * @brief 检查面试是否已经配置题目
     * @throws std::runtime_error 如果题目尚未加载
     */
    void EnsureConfigured() const {
        if (!session.HasQuestions()) {
            throw std::runtime_error("interview has not been configured");
        }
    }

    /**
     * @brief 重置流程标志
     * 每次重新配置题目或重新开始面试时，需要把临时状态恢复到初始值。
     */
    void ResetFlowFlags() {
        intro_answer_received = false;
        waiting_for_followup_answer = false;
    }

    /**
     * @brief 创建统一的动作对象
     * @param type 动作类型
     * @param text 外层要展示、打印或朗读的文本
     * @return InterviewAction
     */
    InterviewAction MakeAction(InterviewActionType type, std::string text) const {
        InterviewAction action;
        action.type = type;
        action.text = std::move(text);
        action.question_index = session.GetCurrentQuestionIndex();
        return action;
    }

    // 成员变量
    InterviewSession& session;
    ResumeParser& resume_parser;
    QuestionGenerator& question_generator;
    AnswerEvaluator& answer_evaluator;
    SummaryGenerator& summary_generator;

    bool intro_answer_received = false;
    bool waiting_for_followup_answer = false;
};

// 构造函数
InterviewOrchestrator::InterviewOrchestrator(
    InterviewSession& session,
    ResumeParser& resume_parser,
    QuestionGenerator& question_generator,
    AnswerEvaluator& answer_evaluator,
    SummaryGenerator& summary_generator)
    : pImpl(std::make_unique<Impl>(
          session,
          resume_parser,
          question_generator,
          answer_evaluator,
          summary_generator)) {}

InterviewOrchestrator::~InterviewOrchestrator() = default;

InterviewOrchestrator::InterviewOrchestrator(InterviewOrchestrator&&) noexcept = default;

InterviewOrchestrator& InterviewOrchestrator::operator=(InterviewOrchestrator&&) noexcept = default;

// 配置根据简历面试
void InterviewOrchestrator::ConfigureResumeInterview(
    const std::string& resume_path,
    int min_questions) {
    pImpl->ConfigureResumeInterview(resume_path, min_questions);
}

// 配置默认面试
void InterviewOrchestrator::ConfigureDefaultInterview(int min_questions) {
    pImpl->ConfigureDefaultInterview(min_questions);
}

InterviewAction InterviewOrchestrator::Start() {
    return pImpl->Start();
}

InterviewAction InterviewOrchestrator::ProcessCandidateAnswer(const std::string& answer) {
    return pImpl->ProcessCandidateAnswer(answer);
}

} // namespace core
} // namespace interview
