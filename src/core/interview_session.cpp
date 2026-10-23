#include "core/interview_session.h"

#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace interview {
namespace core {
namespace {

/**
 * @brief 跨平台安全获取本地时间
 * @param time 原始 time_t 时间戳
 * @return std::tm 本地时间结构
 *
 * Windows 使用 localtime_s，Linux/macOS 使用 localtime_r。
 * 这里统一封装，避免平台细节泄漏到头文件。
 */
std::tm SafeLocalTime(std::time_t time) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    return tm;
}

} // namespace

/**
 * @brief InterviewSession 的私有实现类
 */
class InterviewSession::Impl {
public:
    /**
     * @brief 构造内部状态
     * @param name 候选人姓名
     */
    explicit Impl(std::string name)
        : candidate_name(std::move(name)),
          start_time(std::time(nullptr)) {}

    /**
     * @brief 格式化指定索引的题目
     * @param index 题目索引，从 0 开始
     * @return 带题号前缀的题目文本
     * @throws std::out_of_range 当 index 超出题目范围时抛出
     */
    std::string FormatQuestion(int index) const {
        // 异常处理
        if (index < 0 || index >= static_cast<int>(questions.size())) {
            throw std::out_of_range("question index out of range");
        }

        return "第" + std::to_string(index + 1) + "题：" + questions[index].question;
    }

    /**
     * @brief 生成回答记录时间戳
     * @return 当前时间字符串，格式为 HH:MM:SS
     */
    static std::string MakeTimestamp() {
        auto now = std::time(nullptr);
        auto tm = SafeLocalTime(now);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        return oss.str();
    }

    std::string candidate_name;               // 候选人姓名，用于报告和展示
    std::string resume_text;                  // 简历纯文本，可为空
    std::vector<InterviewQuestion> questions; // 本次面试的问题列表
    std::vector<InterviewRecord> records;     // 已记录的回答和评分

    int current_question_index = 0;           // 当前问题索引，从 0 开始
    int total_score = 0;                      // 所有回答分数累计值
    std::time_t start_time = 0;               // 面试开始时间

    InterviewStage stage = InterviewStage::kNotStarted;
    std::optional<std::string> followup_question;
};

InterviewSession::InterviewSession(std::string candidate_name)
    : pImpl(std::make_unique<Impl>(std::move(candidate_name))) {}

InterviewSession::~InterviewSession() = default;

InterviewSession::InterviewSession(InterviewSession&&) noexcept = default;

InterviewSession& InterviewSession::operator=(InterviewSession&&) noexcept = default;

/**
 * @brief 设置面试题列表
 * @param questions 外部生成好的题目列表
 * 重新设置题目时，会重置面试进度、回答记录、总分和追问状态。
 */
void InterviewSession::SetQuestions(std::vector<InterviewQuestion> questions) {
    pImpl->questions = std::move(questions);
    pImpl->records.clear();
    pImpl->current_question_index = 0;
    pImpl->total_score = 0;
    pImpl->followup_question.reset();
    pImpl->stage = InterviewStage::kNotStarted;
    pImpl->start_time = std::time(nullptr);
}

/**
 * @brief 保存简历纯文本
 * @param resume_text ResumeParser 解析出的文本，可为空
 */
void InterviewSession::SetResumeText(std::string resume_text) {
    pImpl->resume_text = std::move(resume_text);
}

/**
 * @brief 获取开场白提示词
 * @return 面试官开场白文本
 *
 * 这里只返回文本，不调用 TTS，也不直接输出到终端。
 */
std::string InterviewSession::GetIntroPrompt() const {
    return "现在开始C++技术面试。请面试官简短问好，并邀请候选人做自我介绍。";
}

/**
 * @brief 获取第一道题，并进入正式提问阶段
 * @return 格式化后的第一道题
 * @throws std::runtime_error 如果题目列表为空
 */
std::string InterviewSession::GetFirstQuestion() {
    if (pImpl->questions.empty()) {
        throw std::runtime_error("no interview questions loaded");
    }

    pImpl->current_question_index = 0;
    pImpl->stage = InterviewStage::kQuestioning;
    return pImpl->FormatQuestion(pImpl->current_question_index);
}

/**
 * @brief 推进到下一道题
 * @return 下一道题文本；如果题目已经问完，返回空字符串
 * @throws std::runtime_error 如果题目列表为空
 */
std::string InterviewSession::GetNextQuestion() {
    if (pImpl->questions.empty()) {
        throw std::runtime_error("no interview questions loaded");
    }

    ++pImpl->current_question_index;
    pImpl->followup_question.reset();

    if (pImpl->current_question_index >= static_cast<int>(pImpl->questions.size())) {
        pImpl->stage = InterviewStage::kCompleted;
        return "";
    }

    pImpl->stage = InterviewStage::kQuestioning;
    return pImpl->FormatQuestion(pImpl->current_question_index);
}

/**
 * @brief 获取并消费当前追问
 * @return 追问文本；如果没有追问，返回空字符串
 * 调用后会清空追问缓存，避免同一个追问被重复发送
 */
std::string InterviewSession::GetFollowUpQuestion() {
    if (!pImpl->followup_question.has_value()) {
        return "";
    }

    std::string result = "追问：" + *pImpl->followup_question;
    pImpl->followup_question.reset();
    pImpl->stage = InterviewStage::kFollowUp;
    return result;
}

/**
 * @brief 记录一次候选人回答
 * @param answer 候选人回答文本
 * @param evaluation 外部评分器生成的评分结果
 * @param is_followup true 表示这次回答来自追问
 * 本函数只记录结果，不直接调用 LLM
 * 如果普通题回答需要追问，会把追问文本暂存在 followup_question 中
 */
void InterviewSession::RecordAnswer(const std::string& answer,
                                    const AnswerEvaluation& evaluation,
                                    bool is_followup) {
    if (pImpl->questions.empty() ||
        pImpl->current_question_index >= static_cast<int>(pImpl->questions.size())) {
        throw std::runtime_error("no current question available");
    }

    InterviewRecord record;
    record.question = pImpl->questions[pImpl->current_question_index];
    record.answer = answer;
    record.timestamp = Impl::MakeTimestamp();
    record.evaluation = evaluation;
    record.is_followup = is_followup;

    pImpl->records.push_back(std::move(record));
    pImpl->total_score += evaluation.score;

    // 只允许普通题触发追问，追问回答不再继续制造追问，避免流程无法结束。
    if (!is_followup && evaluation.need_followup && !evaluation.followup_question.empty()) {
        pImpl->followup_question = evaluation.followup_question;
    }
}

/**
 * @brief 获取最后一次回答分数
 * @return 如果没有回答记录返回 0，否则返回最后一条记录的分数
 */
int InterviewSession::GetLastScore() const {
    if (pImpl->records.empty()) {
        return 0;
    }

    return pImpl->records.back().evaluation.score;
}

/**
 * @brief 判断当前是否存在待发送追问
 * @return true 表示有追问，false 表示没有追问
 */
bool InterviewSession::ShouldFollowUp() const {
    return pImpl->followup_question.has_value();
}

/**
 * @brief 判断面试是否已经完成
 * @return true 表示所有题目已经处理完
 */
bool InterviewSession::isComplete() const {
    return pImpl->stage == InterviewStage::kCompleted;
}

/**
 * @brief 判断是否已经加载题目
 * @return true 表示题目列表非空
 */
bool InterviewSession::HasQuestions() const {
    return !pImpl->questions.empty();
}

/**
 * @brief 获取当前题目索引
 * @return 当前题目索引，从 0 开始
 */
int InterviewSession::GetCurrentQuestionIndex() const {
    return pImpl->current_question_index;
}

/**
 * @brief 获取当前总分
 * @return 所有回答分数累计值
 */
int InterviewSession::GetTotalScore() const {
    return pImpl->total_score;
}

/**
 * @brief 计算面试持续时间
 * @param now 当前时间戳，由外部传入便于测试固定时间
 * @return 从会话开始到 now 的分钟数
 */
double InterviewSession::GetDurationMinutes(std::time_t now) const {
    return std::difftime(now, pImpl->start_time) / 60.0;
}

/**
 * @brief 获取候选人姓名
 * @return 候选人姓名引用
 */
const std::string& InterviewSession::GetCandidateName() const {
    return pImpl->candidate_name;
}

/**
 * @brief 获取简历文本
 * @return 简历文本引用，可为空
 */
const std::string& InterviewSession::GetResumeText() const {
    return pImpl->resume_text;
}

/**
 * @brief 获取当前题目
 * @return 当前题目只读引用
 * @throws std::runtime_error 如果尚未加载题目或当前索引无效
 */
const InterviewQuestion& InterviewSession::GetCurrentQuestion() const {
    if (pImpl->questions.empty() ||
        pImpl->current_question_index >= static_cast<int>(pImpl->questions.size())) {
        throw std::runtime_error("no current question available");
    }

    return pImpl->questions[pImpl->current_question_index];
}

/**
 * @brief 获取所有回答记录
 * @return 回答记录列表只读引用
 */
const std::vector<InterviewRecord>& InterviewSession::GetRecords() const {
    return pImpl->records;
}

/**
 * @brief 获取当前面试阶段
 * @return 当前 InterviewStage
 */
InterviewStage InterviewSession::GetStage() const {
    return pImpl->stage;
}

} // namespace core
} // namespace interview
