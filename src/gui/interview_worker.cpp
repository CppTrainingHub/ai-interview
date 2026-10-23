#include "gui/interview_worker.h"

#include <stdexcept>
#include <utility>

#include <QMetaObject>

#include "runtime/interview_runtime.h"

namespace interview {
namespace gui {

class InterviewWorker::Impl {
public:
    Impl(InterviewWorker& owner_value, config::AppConfig config_value)
        : owner(owner_value), base_config(std::move(config_value)) {}

    void StartInterview(GuiInterviewConfig gui_config) {
        try {
            StopInterview();
            emit owner.StateChanged(GuiInterviewState::kPreparing);

            audio_mode = gui_config.audio_mode;
            auto runtime_config = base_config;
            runtime_config.runtime_mode = audio_mode ? "audio" : "text";
            runtime = std::make_unique<runtime::InterviewRuntime>(
                std::move(runtime_config), gui_config.candidate_name.toStdString());

            const auto action = runtime->Start(
                gui_config.resume_path.toStdString(), gui_config.min_questions);
            DispatchAction(action);
        } catch (const std::exception& exception) {
            Fail(exception.what());
        }
    }

    void BeginAudioAnswer() {
        try {
            RequireRuntime();
            if (!audio_mode) {
                throw std::runtime_error("audio answer is unavailable in text mode");
            }
            emit owner.StateChanged(GuiInterviewState::kRecording);
            runtime->BeginAsyncAudioAnswer(
                [this](const core::InterviewAction& action) {
                    QMetaObject::invokeMethod(
                        &owner,
                        [this, action]() { DispatchAction(action); },
                        Qt::QueuedConnection);
                },
                [this](const std::string& error) {
                    QMetaObject::invokeMethod(
                        &owner,
                        [this, error]() { Fail(error); },
                        Qt::QueuedConnection);
                },
                [this](const std::string& transcript) {
                    emit owner.TranscriptReady(QString::fromStdString(transcript));
                    emit owner.StateChanged(GuiInterviewState::kEvaluating);
                });
        } catch (const std::exception& exception) {
            Fail(exception.what());
        }
    }

    void EndAudioAnswer() {
        try {
            RequireRuntime();
            emit owner.StateChanged(GuiInterviewState::kStoppingRecording);
            runtime->EndAsyncAudioAnswer();
            emit owner.StateChanged(GuiInterviewState::kRecognizing);
        } catch (const std::exception& exception) {
            Fail(exception.what());
        }
    }

    void SubmitTextAnswer(const QString& answer) {
        try {
            RequireRuntime();
            const auto trimmed = answer.trimmed();
            if (trimmed.isEmpty()) {
                throw std::runtime_error("answer must not be empty");
            }
            emit owner.TranscriptReady(trimmed);
            emit owner.StateChanged(GuiInterviewState::kEvaluating);
            DispatchAction(runtime->SubmitTextAnswer(trimmed.toStdString()));
        } catch (const std::exception& exception) {
            Fail(exception.what());
        }
    }

    void StopInterview() {
        if (!runtime) {
            return;
        }
        try {
            if (audio_mode) {
                runtime->EndAsyncAudioAnswer();
                runtime->WaitForAudioOperation();
            }
        } catch (...) {
        }
        runtime.reset();
        emit owner.StateChanged(GuiInterviewState::kReady);
    }

private:
    void DispatchAction(const core::InterviewAction& action) {
        if (!runtime) {
            return;
        }
        emit owner.ActionReady(ToGuiAction(action));

        if (!audio_mode || action.text.empty()) {
            CompleteAction(action);
            return;
        }

        emit owner.StateChanged(GuiInterviewState::kInterviewerSpeaking);
        runtime->SpeakActionAsync(
            action,
            [this, action]() {
                QMetaObject::invokeMethod(
                    &owner,
                    [this, action]() { CompleteAction(action); },
                    Qt::QueuedConnection);
            },
            [this](const std::string& error) {
                QMetaObject::invokeMethod(
                    &owner,
                    [this, error]() { Fail(error); },
                    Qt::QueuedConnection);
            });
    }

    void CompleteAction(const core::InterviewAction& action) {
        if (action.type == core::InterviewActionType::kFinish) {
            try {
                emit owner.ReportReady(QString::fromStdString(runtime->GenerateReport("gui")));
                emit owner.StateChanged(GuiInterviewState::kCompleted);
            } catch (const std::exception& exception) {
                Fail(exception.what());
            }
            return;
        }
        emit owner.StateChanged(GuiInterviewState::kWaitingForAnswer);
    }

    void RequireRuntime() const {
        if (!runtime) {
            throw std::runtime_error("interview has not been started");
        }
    }

    void Fail(const std::string& error) {
        emit owner.StateChanged(GuiInterviewState::kError);
        emit owner.ErrorOccurred(QString::fromStdString(error));
    }

    InterviewWorker& owner;
    config::AppConfig base_config;
    std::unique_ptr<runtime::InterviewRuntime> runtime;
    bool audio_mode = false;
};

InterviewWorker::InterviewWorker(config::AppConfig config, QObject* parent)
    : QObject(parent), pImpl(std::make_unique<Impl>(*this, std::move(config))) {}
InterviewWorker::~InterviewWorker() = default;
void InterviewWorker::StartInterview(GuiInterviewConfig config) {
    pImpl->StartInterview(std::move(config));
}
void InterviewWorker::BeginAudioAnswer() { pImpl->BeginAudioAnswer(); }
void InterviewWorker::EndAudioAnswer() { pImpl->EndAudioAnswer(); }
void InterviewWorker::SubmitTextAnswer(QString answer) { pImpl->SubmitTextAnswer(answer); }
void InterviewWorker::StopInterview() { pImpl->StopInterview(); }

} // namespace gui
} // namespace interview
