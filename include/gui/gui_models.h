#pragma once

#include <QString>
#include <QMetaType>

#include "core/interview_orchestrator.h"

namespace interview {
namespace gui {

/**
 * @brief GUI 会话启动参数。
 * @note 该值对象只携带用户输入，不包含 Runtime 或 Qt 控件所有权。
 */
struct GuiInterviewConfig {
    QString candidate_name;
    QString resume_path;
    int min_questions = 5;
    bool audio_mode = true;
};

/** @brief GUI 可展示的面试动作。 */
struct GuiInterviewAction {
    core::InterviewActionType type = core::InterviewActionType::kNone;
    QString text;
    int question_index = 0;
};

/** @brief GUI 会话状态，与 Core 和音频内部状态解耦。 */
enum class GuiInterviewState {
    kReady,
    kPreparing,
    kInterviewerSpeaking,
    kWaitingForAnswer,
    kRecording,
    kStoppingRecording,
    kRecognizing,
    kEvaluating,
    kCompleted,
    kError
};

/**
 * @brief 将 Core 动作转换为 Qt 可传递的数据对象。
 * @param action Core 层产生的不可变面试动作。
 * @return 可通过 Qt 信号槽传递的 GUI 动作副本。
 */
inline GuiInterviewAction ToGuiAction(const core::InterviewAction& action) {
    return {action.type, QString::fromStdString(action.text), action.question_index};
}

} // namespace gui
} // namespace interview

Q_DECLARE_METATYPE(interview::gui::GuiInterviewConfig)
Q_DECLARE_METATYPE(interview::gui::GuiInterviewAction)
Q_DECLARE_METATYPE(interview::gui::GuiInterviewState)
