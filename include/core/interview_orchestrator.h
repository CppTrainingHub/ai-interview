#pragma once

#include <memory>
#include <string>

#include "core/interview_ports.h"
#include "core/interview_session.h"

/**
 * @brief 这个模块主要工作流程是：
 *        外部收到候选人文本
            -> ProcessCandidateAnswer(text)
            -> 返回 AskQuestion / AskFollowUp / Finish
            -> 外部决定显示到 UI、打印到 CLI、还是发给 TTS
 */

namespace interview {
namespace core {

/**
 * @brief Orchestrator 每处理一次输入，返回一个动作
 *        由外层根据动作决定如何展示或者朗读
 */
enum class InterviewActionType {
    kNone,
    kSpeakIntro,
    kAskQuestion,
    kAskFollowUp,
    kFinish
};

struct InterviewAction {
    InterviewActionType type = InterviewActionType::kNone;
    std::string text;
    int question_index = 0;
};

/**
 * @brief 处理收到候选人的文本之后下一步干什么
 */
class InterviewOrchestrator {
public:
    /**
     * @brief 构造面试流程编排器
     * @param session 面试状态容器，负责保存题目和回答记录
     * @param resume_parser 简历解析端口，由外层提供具体实现
     * @param question_generator 题目生成端口，由外层提供LLM或规则实现
     * @param answer_evaluator 回答评分端口，由外层提供LLM或规则实现
     * @param summary_generator 总结生成端口，由外层提供具体实现
     * @note Orchestrator 不拥有这些对象，只保存引用；调用方需保证生命周期足够长。
     */
    InterviewOrchestrator(
        InterviewSession& session,
        ResumeParser& resume_parser,
        QuestionGenerator& question_generator,
        AnswerEvaluator& answer_evaluator,
        SummaryGenerator& summary_generator
    );

    /**
     * @brief 析构函数
     */
    ~InterviewOrchestrator();

    InterviewOrchestrator(const InterviewOrchestrator&) = delete;
    InterviewOrchestrator& operator=(const InterviewOrchestrator&) = delete;

    InterviewOrchestrator(InterviewOrchestrator&&) noexcept;
    InterviewOrchestrator& operator=(InterviewOrchestrator&&) noexcept;

    /**
     * @brief 简历驱动模式
     *        解析简历->生成题目->加载到Session
     * @param resume_path 简历文件路径
     * @param min_questions 希望生成的最少题目数量
     */
    void ConfigureResumeInterview(
        const std::string& resume_path,
        int min_questions
    );

    /**
     * @brief 默认模式，生成一些C++通用题目->加载到session
     * @param min_questions 希望生成的最少题目数量
     */
    void ConfigureDefaultInterview(int min_questions);

    /**
     * @brief 开始面试
     * @return kSpeakIntro 动作，text 为开场白提示词
     * @throws std::runtime_error 如果还没有配置题目
     */
    InterviewAction Start();

    /**
     * @brief 处理候选人文本回答，返回下一步动作
     * @param answer 候选人的文本回答，可以来自CLI输入或ASR结果
     * @return 下一步动作：提问、追问或结束
     */
    InterviewAction ProcessCandidateAnswer(const std::string& answer);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};


}
}
