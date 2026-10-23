#pragma once

#include <memory>

#include <QObject>

#include "config/app_config.h"
#include "gui/gui_models.h"

namespace interview {
namespace gui {

/**
 * @brief 在非 UI 线程中驱动 InterviewRuntime。
 * @note 所有公开槽应通过 queued connection 调用；该对象不创建或修改控件。
 */
class InterviewWorker final : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 创建 Runtime 工作线程适配器。
     * @param config 应用配置副本，后续用于构造每场面试的 Runtime。
     * @param parent 可选 QObject 所有者；移动线程前通常应为空。
     */
    explicit InterviewWorker(config::AppConfig config, QObject* parent = nullptr);
    ~InterviewWorker() override;

    InterviewWorker(const InterviewWorker&) = delete;
    InterviewWorker& operator=(const InterviewWorker&) = delete;

public slots:
    /**
     * @brief 创建 Runtime 并启动一场面试。
     * @param config 已由配置对话框校验的会话参数。
     */
    void StartInterview(interview::gui::GuiInterviewConfig config);
    /** @brief 开始采集语音回答。 */
    void BeginAudioAnswer();
    /** @brief 请求结束语音回答。 */
    void EndAudioAnswer();
    /**
     * @brief 提交文本回答。
     * @param answer 非空候选人回答；函数会再次执行去空白校验。
     */
    void SubmitTextAnswer(QString answer);
    /** @brief 停止后台操作并销毁当前 Runtime。 */
    void StopInterview();

signals:
    void ActionReady(interview::gui::GuiInterviewAction action);
    void StateChanged(interview::gui::GuiInterviewState state);
    void TranscriptReady(QString transcript);
    void ReportReady(QString path);
    void ErrorOccurred(QString message);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace gui
} // namespace interview
