#pragma once

#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/interview_models.h"

namespace interview {
namespace core {
/**
 * @brief InterviewSession 是一次面试的状态容器
 * @note 主要负责：
 *          题目列表、当前题号、回答记录、追问状态、候选人信息
 *       不会做
 *          PDF 解析、LLM 调用、音频、WebSocket、报告写文件
 */
class InterviewSession {
public:
    /**
     * @brief 构造一个面试会话
     * @param candidate_name 候选人姓名
     */
    explicit InterviewSession(std::string candidate_name = "Candidate");

    /**
     * @brief 以下是析构函数、拷贝构造函数以及移动构造
     */
    ~InterviewSession();

    InterviewSession(const InterviewSession&) = delete;
    InterviewSession& operator=(const InterviewSession&) = delete;

    InterviewSession(InterviewSession&&) noexcept;
    InterviewSession& operator=(InterviewSession&&) noexcept;

    /**
     * @brief 设置面试问题
     * @param question 问题
     */
    void SetQuestions(std::vector<InterviewQuestion> questions);

    /**
     * @brief 保存简历文本
     * @param resume_text 简历解析后的纯文本内容，可为空
     */
    void SetResumeText(std::string resume_text);

    /**
     * @brief 获取开场白提示词
     * @return 面试官开场白文本
     */
    std::string GetIntroPrompt() const;

    /**
     * @brief 获取第一道面试题，并进入提问阶段
     * @return 格式化后的第一道题文本
     * @throws std::runtime_error 如果尚未加载任何题目
     */
    std::string GetFirstQuestion();

    /**
     * @brief 推进到下一个题目
     * @return 格式化后的第一道题文本，如果没有下一题，返回空字符串
     * @throws std::runtime_error 如果尚未加载任何题目
     */
    std::string GetNextQuestion();

    /**
     * @brief 获取当前待发送的追问问题
     *        调用之后会清空内部追问状态，避免重复追问
     * @return 格式化后的追问文本，如果没有追问，则返回空字符串
     */
    std::string GetFollowUpQuestion();

    /**
     * @brief 记录候选人的一次回答
     * @param answer 候选人回答文本
     * @param evaluation 对该回答的评分结果
     * @param is_followup 当前回答是否为追问回答
     * @note 如果 evaluation.need_followup 为 true，实现层会保存追问问题，等待外部获取。
     */
    void RecordAnswer(const std::string& answer,
                      const AnswerEvaluation& evaluation,
                      bool is_followup = false);

    /**
     * @brief 获取最后一次回答的分数
     * @return 最后一次回答的分数，如果没有记录，则返回0
     */
    int GetLastScore() const;

    /**
     * @brief 判断当前是否有待发送的追问
     * @return true 表示需要追问，fasle表示不需要追问
     */
    bool ShouldFollowUp() const;

    /**
     * @brief 判断面试是否完成
     * @return true 表示面试已完成，false表示仍有题目
     */
    bool isComplete() const;

    /**
     * @brief 判断是否已经加载题目
     * @return true 表示题目列表非空，false 表示尚未加载题目
     */
    bool HasQuestions() const;

    /**
     * @brief 获取当前题目索引
     * @return 当前题目索引，从0开始
     */
    int GetCurrentQuestionIndex() const;

    /**
     * @brief 获取当前总分
     * @return 所有回答评分的总和
     */
    int GetTotalScore() const;

    /**
     * @brief 获取面试持续时间
     * @param now 当前时间戳
     * @return 从会话开始到now的分钟数
     */
    double GetDurationMinutes(std::time_t now) const;

    /**
     * @brief 获取候选人姓名
     * @return 候选人姓名引用
     */
    const std::string& GetCandidateName() const;

    /**
     * @brief 获取简历文本
     * @return 简历文本引用，可为空
     */
    const std::string& GetResumeText() const;

    /**
     * @brief 获取当前题目
     * @return 当前题目的只读引用
     * @throws std::runtime_error 如果尚未加载题目或当前索引无效
     */
    const InterviewQuestion& GetCurrentQuestion() const;

    /**
     * @brief 获取所有回答记录
     * @return 回答记录列表的只读引用
     */
    const std::vector<InterviewRecord>& GetRecords() const;

    /**
     * @brief 获取当前面试阶段
     * @return 当前 InterviewStage
     */
    InterviewStage GetStage() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}
}
