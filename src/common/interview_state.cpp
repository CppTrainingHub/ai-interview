#include "common/interview_state.h"
#include "common/logger.h"

namespace interview {
namespace common {

InterviewStateMachine::InterviewStateMachine()
    : current_state_(InterviewState::kIdle)
    , callback_(nullptr) {
    LOG_INFO("InterviewStateMachine initialized");
}

InterviewStateMachine& InterviewStateMachine::Instance() {
    static InterviewStateMachine instance;
    return instance;
}

InterviewState InterviewStateMachine::GetState() const {
    return current_state_.load(std::memory_order_acquire);
}

void InterviewStateMachine::SetState(InterviewState new_state) {
    InterviewState old_state = current_state_.exchange(new_state, std::memory_order_acq_rel);
    
    if (old_state == new_state) {
        return; // 状态未改变，不触发回调
    }
    
    LOG_INFO("State transition: {} -> {}", GetStateName(old_state), GetStateName(new_state));
    
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        try {
            callback_(old_state, new_state);
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in state change callback: {}", e.what());
        } catch (...) {
            LOG_ERROR("Unknown exception in state change callback");
        }
    }
}

void InterviewStateMachine::SetStateChangeCallback(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_ = callback;
}

void InterviewStateMachine::ClearStateChangeCallback() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_ = nullptr;
}

void InterviewStateMachine::Reset() {
    SetState(InterviewState::kIdle);
    LOG_INFO("InterviewStateMachine reset to Idle");
}


const char* InterviewStateMachine::GetStateName(InterviewState state) {
    switch (state) {
        case InterviewState::kConnecting:
            return "连接服务器中";
        case InterviewState::kInterviewerSpeaking:
            return "面试官说话中";
        case InterviewState::kIdle:
            return "等待候选人说话";
        case InterviewState::kCandidateSpeaking:
            return "候选人说话中";
        case InterviewState::kInterviewerThinking:
            return "面试官思考";
        case InterviewState::kSessionEnding:
            return "会话结束";
        case InterviewState::kCompleted:
            return "完成";
        case InterviewState::kError:
            return "错误";
        default:
            return "未知状态";
    }
}

} // namespace common
} // namespace interview

