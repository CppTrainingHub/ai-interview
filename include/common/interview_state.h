#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <mutex>

namespace interview {
namespace common {

/**
 * @brief 面试会话状态枚举
 */
enum class InterviewState {
    kConnecting = 0,           // 连接服务器中（启动）
    kInterviewerSpeaking,      // 面试官说话（TTS）
    kIdle,                    // 空闲（等待候选人说话）
    kCandidateSpeaking,       // 候选人说话（录音）
    kInterviewerThinking,     // 面试官思考/生成新问题
    kSessionEnding,           // 会话结束（正在收尾）
    kCompleted,               // 会话完成
    kError                    // 错误/异常
};

/**
 * @brief 全局单例状态机
 */
class InterviewStateMachine {
public:
    /**
     * @brief 状态变化回调函数类型
     * @param old_state 旧状态
     * @param new_state 新状态
     */
    using StateChangeCallback = std::function<void(InterviewState old_state, InterviewState new_state)>;

    /**
     * @brief 获取单例实例
     * @return 全局唯一的状态机实例
     */
    static InterviewStateMachine& Instance();

    /**
     * @brief 获取当前状态（线程安全）
     * @return 当前面试状态
     */
    InterviewState GetState() const;

    /**
     * @brief 设置新状态（线程安全）
     * 
     * 如果状态发生变化，会触发观察者回调
     * 
     * @param new_state 新的面试状态
     */
    void SetState(InterviewState new_state);

    /**
     * @brief 注册状态变化回调
     * 
    * 同一时间只支持一个回调（通常是UI层）
     * 多次调用会覆盖之前的回调
     * 
     * @param callback 状态变化时的回调函数
     */
    void SetStateChangeCallback(StateChangeCallback callback);

    /**
     * @brief 清除状态变化回调
     */
    void ClearStateChangeCallback();

    /**
     * @brief 重置状态机到初始状态
     */
    void Reset();

    /**
     * @brief 获取状态的中文名称
     * @param state 面试状态
     * @return 状态的中文描述
     */
    static const char* GetStateName(InterviewState state);

    InterviewStateMachine(const InterviewStateMachine&) = delete;
    InterviewStateMachine& operator=(const InterviewStateMachine&) = delete;

private:
    InterviewStateMachine();
    ~InterviewStateMachine() = default;

    std::atomic<InterviewState> current_state_;
    std::mutex callback_mutex_;
    StateChangeCallback callback_;
};

} // namespace common
} // namespace interview

