#include "audio/async_speech_controller.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace interview {
namespace audio {

class AsyncSpeechController::Impl {
public:
    explicit Impl(SpeechPipeline& pipeline_value) : pipeline(pipeline_value) {}
    ~Impl() {
        stop_requested = true;
        Wait();
    }

    void StartCapture(TranscriptCallback on_transcript, ErrorCallback on_error) {
        PrepareOperation();
        stop_requested = false;
        worker = std::thread([this,
                              transcript = std::move(on_transcript),
                              error = std::move(on_error)]() mutable {
            try {
                ChangeState(AsyncSpeechState::kCapturing);
                pipeline.BeginAnswerCapture();
                while (!stop_requested.load()) {
                    pipeline.CaptureAnswerChunk();
                    std::this_thread::yield();
                }
                ChangeState(AsyncSpeechState::kRecognizing);
                const auto text = pipeline.FinishAnswerCapture();
                ChangeState(AsyncSpeechState::kIdle);
                if (transcript) {
                    transcript(text);
                }
            } catch (const std::exception& exception) {
                pipeline.CancelAnswerCapture();
                ChangeState(AsyncSpeechState::kError);
                if (error) {
                    error(exception.what());
                }
            }
        });
    }

    void RequestStopCapture() {
        const auto current = State();
        if (current == AsyncSpeechState::kCapturing) {
            ChangeState(AsyncSpeechState::kStopping);
        }
        if (current == AsyncSpeechState::kCapturing ||
            current == AsyncSpeechState::kStopping ||
            current == AsyncSpeechState::kIdle) {
            stop_requested = true;
        }
    }

    void Speak(std::string text,
               CompletionCallback on_complete,
               ErrorCallback on_error) {
        if (text.empty()) {
            throw std::runtime_error("speech text must not be empty");
        }
        PrepareOperation();
        worker = std::thread([this,
                              value = std::move(text),
                              complete = std::move(on_complete),
                              error = std::move(on_error)]() mutable {
            try {
                ChangeState(AsyncSpeechState::kSpeaking);
                pipeline.SpeakText(value);
                ChangeState(AsyncSpeechState::kIdle);
                if (complete) {
                    complete();
                }
            } catch (const std::exception& exception) {
                ChangeState(AsyncSpeechState::kError);
                if (error) {
                    error(exception.what());
                }
            }
        });
    }

    void Wait() {
        if (worker.joinable() && worker.get_id() != std::this_thread::get_id()) {
            worker.join();
        }
    }

    void SetStateCallback(StateCallback callback) {
        std::lock_guard<std::mutex> lock(mutex);
        state_callback = std::move(callback);
    }

    AsyncSpeechState State() const {
        std::lock_guard<std::mutex> lock(mutex);
        return state;
    }

private:
    void PrepareOperation() {
        Wait();
        const auto current = State();
        if (current != AsyncSpeechState::kIdle && current != AsyncSpeechState::kError) {
            throw std::runtime_error("another asynchronous speech operation is active");
        }
        ChangeState(AsyncSpeechState::kIdle);
    }

    void ChangeState(AsyncSpeechState next) {
        StateCallback callback;
        {
            std::lock_guard<std::mutex> lock(mutex);
            state = next;
            callback = state_callback;
        }
        if (callback) {
            callback(next);
        }
    }

    SpeechPipeline& pipeline;
    std::atomic<bool> stop_requested{false};
    std::thread worker;
    mutable std::mutex mutex;
    AsyncSpeechState state = AsyncSpeechState::kIdle;
    StateCallback state_callback;
};

AsyncSpeechController::AsyncSpeechController(SpeechPipeline& pipeline)
    : pImpl(std::make_unique<Impl>(pipeline)) {}
AsyncSpeechController::~AsyncSpeechController() = default;
void AsyncSpeechController::StartCapture(TranscriptCallback transcript, ErrorCallback error) {
    pImpl->StartCapture(std::move(transcript), std::move(error));
}
void AsyncSpeechController::RequestStopCapture() { pImpl->RequestStopCapture(); }
void AsyncSpeechController::Speak(std::string text,
                                  CompletionCallback complete,
                                  ErrorCallback error) {
    pImpl->Speak(std::move(text), std::move(complete), std::move(error));
}
void AsyncSpeechController::Wait() { pImpl->Wait(); }
void AsyncSpeechController::SetStateCallback(StateCallback callback) {
    pImpl->SetStateCallback(std::move(callback));
}
AsyncSpeechState AsyncSpeechController::State() const { return pImpl->State(); }

} // namespace audio
} // namespace interview
