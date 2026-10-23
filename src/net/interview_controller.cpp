#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "net/interview_controller.h"
#include "runtime/interview_runtime.h"

namespace interview {
namespace net {
namespace {

/**
 * @brief 将 core 面试动作类型转换为网络层字符串
 * @param type core 动作类型
 * @return 可序列化的动作类型字符串
 */
std::string ToActionTypeString(core::InterviewActionType type) {
    switch (type) {
        case core::InterviewActionType::kSpeakIntro:
            return "speak_intro";
        case core::InterviewActionType::kAskQuestion:
            return "ask_question";
        case core::InterviewActionType::kAskFollowUp:
            return "ask_followup";
        case core::InterviewActionType::kFinish:
            return "finish";
        case core::InterviewActionType::kNone:
            return "none";
    }

    return "unknown";
}

/**
 * @brief 根据动作构造网络响应
 * @param session_id 面试会话 ID
 * @param action core 层返回的动作
 * @return 网络层统一响应
 */
InterviewNetResponse MakeActionResponse(const std::string& session_id,
                                        const core::InterviewAction& action) {
    InterviewNetResponse response;
    response.ok = true;
    response.session_id = session_id;
    response.action_type = ToActionTypeString(action.type);
    response.text = action.text;
    response.question_index = action.question_index;
    response.finished = action.type == core::InterviewActionType::kFinish;
    return response;
}

/**
 * @brief 构造错误响应
 * @param message 错误信息
 * @return 网络层统一响应
 */
InterviewNetResponse MakeErrorResponse(const std::string& message) {
    InterviewNetResponse response;
    response.ok = false;
    response.error = message;
    return response;
}

/**
 * @brief 生成简单会话 ID
 * @return 会话 ID 字符串
 * @note 使用递增 ID；后续可以替换为 UUID
 */
std::string MakeSessionId() {
    static int counter = 0;
    ++counter;
    return "session-" + std::to_string(counter);
}

} // namespace

class InterviewController::Impl {
public:
    /**
     * @brief 构造控制器实现
     * @param app_config 应用配置
     */
    explicit Impl(config::AppConfig app_config)
        : config(std::move(app_config)) {}

    /**
     * @brief 单个网络会话上下文
     * @note 这些对象存在引用关系，因此使用 unique_ptr 明确生命周期和构造顺序
     */
    struct SessionContext {
        explicit SessionContext(const config::AppConfig& config_value,
                                std::string candidate)
            : candidate_name(std::move(candidate)),
              runtime(config_value, candidate_name) {}

        std::string candidate_name;
        runtime::InterviewRuntime runtime;
    };

    InterviewNetResponse StartInterview(const StartInterviewRequest& request) {
        try {
            if (request.min_questions <= 0) {
                return MakeErrorResponse("min_questions must be positive");
            }

            const std::string session_id = MakeSessionId();
            auto context = std::make_unique<SessionContext>(
                config,
                request.candidate_name.empty() ? "Candidate" : request.candidate_name);

            const auto action = context->runtime.Start(
                request.resume_path,
                request.min_questions);
            sessions.emplace(session_id, std::move(context));

            return MakeActionResponse(session_id, action);
        } catch (const std::exception& e) {
            return MakeErrorResponse(e.what());
        }
    }

    InterviewNetResponse SubmitAnswer(const SubmitAnswerRequest& request) {
        try {
            auto* context = FindSession(request.session_id);
            if (context == nullptr) {
                return MakeErrorResponse("session not found: " + request.session_id);
            }

            if (request.answer.empty()) {
                return MakeErrorResponse("answer must not be empty");
            }

            const auto action = context->runtime.SubmitTextAnswer(request.answer);

            return MakeActionResponse(request.session_id, action);
        } catch (const std::exception& e) {
            return MakeErrorResponse(e.what());
        }
    }

    InterviewNetResponse GenerateReport(const GenerateReportRequest& request) {
        try {
            auto* context = FindSession(request.session_id);
            if (context == nullptr) {
                return MakeErrorResponse("session not found: " + request.session_id);
            }

            const std::string written_path = context->runtime.GenerateReport(request.session_id);

            InterviewNetResponse response;
            response.ok = true;
            response.session_id = request.session_id;
            response.report_path = written_path;
            return response;
        } catch (const std::exception& e) {
            return MakeErrorResponse(e.what());
        }
    }

private:
    SessionContext* FindSession(const std::string& session_id) {
        const auto it = sessions.find(session_id);
        if (it == sessions.end()) {
            return nullptr;
        }

        return it->second.get();
    }

    config::AppConfig config;
    std::unordered_map<std::string, std::unique_ptr<SessionContext>> sessions;
};

InterviewController::InterviewController(config::AppConfig config)
    : pImpl(std::make_unique<Impl>(std::move(config))) {}

InterviewController::~InterviewController() = default;

InterviewController::InterviewController(InterviewController&&) noexcept = default;

InterviewController& InterviewController::operator=(InterviewController&&) noexcept = default;

InterviewNetResponse InterviewController::StartInterview(
    const StartInterviewRequest& request) {
    return pImpl->StartInterview(request);
}

InterviewNetResponse InterviewController::SubmitAnswer(
    const SubmitAnswerRequest& request) {
    return pImpl->SubmitAnswer(request);
}

InterviewNetResponse InterviewController::GenerateReport(
    const GenerateReportRequest& request) {
    return pImpl->GenerateReport(request);
}

} // namespace net
} // namespace interview
