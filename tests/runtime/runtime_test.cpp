#include <cassert>
#include <filesystem>
#include <string>
#include <atomic>

#include "config/app_config.h"
#include "core/interview_models.h"
#include "runtime/interview_runtime.h"

namespace {

interview::config::AppConfig MakeScriptedConfig() {
    interview::config::AppConfig config;
    config.dependency_mode = "scripted";
    config.runtime_mode = "text";
    config.report_output_dir =
        (std::filesystem::temp_directory_path() / "ai_interview_runtime_test").string();
    return config;
}

void TestScriptedRuntimeFullFlow() {
    auto config = MakeScriptedConfig();
    interview::runtime::InterviewRuntime runtime(config, "Alice");

    auto action = runtime.Start("", 2);
    assert(action.type == interview::core::InterviewActionType::kSpeakIntro);

    action = runtime.SubmitTextAnswer("I am Alice and I like C++ backend projects.");
    assert(action.type == interview::core::InterviewActionType::kAskQuestion);

    action = runtime.SubmitTextAnswer(
        "RAII binds resource ownership to object lifetime and releases resources automatically.");
    assert(action.type == interview::core::InterviewActionType::kAskQuestion);

    action = runtime.SubmitTextAnswer(
        "vector keeps contiguous storage and growth may invalidate iterators.");
    assert(action.type == interview::core::InterviewActionType::kFinish);
    assert(!action.text.empty());

    const auto report_path = runtime.GenerateReport("unit");
    assert(std::filesystem::exists(report_path));
}

void TestScriptedAudioRuntimeFullFlow() {
    auto config = MakeScriptedConfig();
    config.runtime_mode = "audio";
    config.audio_device = "null";
    config.asr_provider = "scripted";
    config.tts_provider = "silent_pcm";
    config.scripted_transcripts =
        "I am Alice from the audio path.||"
        "RAII binds resource ownership to object lifetime and uses destructors to release resources automatically.||"
        "vector growth can reallocate contiguous storage and invalidate iterators, pointers, and references.";

    interview::runtime::InterviewRuntime runtime(config, "AudioAlice");

    auto action = runtime.Start("", 2);
    runtime.SpeakAction(action);
    assert(action.type == interview::core::InterviewActionType::kSpeakIntro);

    action = runtime.CaptureAndSubmitAudioAnswer();
    runtime.SpeakAction(action);
    assert(action.type == interview::core::InterviewActionType::kAskQuestion);

    action = runtime.CaptureAndSubmitAudioAnswer();
    runtime.SpeakAction(action);
    assert(action.type == interview::core::InterviewActionType::kAskQuestion);

    action = runtime.CaptureAndSubmitAudioAnswer();
    runtime.SpeakAction(action);
    assert(action.type == interview::core::InterviewActionType::kFinish);
}

void TestAsyncAudioRuntimeFlow() {
    auto config = MakeScriptedConfig();
    config.runtime_mode = "audio";
    config.audio_device = "null";
    config.asr_provider = "scripted";
    config.tts_provider = "silent_pcm";
    config.scripted_transcripts = "async introduction";
    interview::runtime::InterviewRuntime runtime(config, "AsyncAlice");
    runtime.Start("", 1);

    std::atomic<bool> completed{false};
    interview::core::InterviewAction next;
    runtime.BeginAsyncAudioAnswer(
        [&completed, &next](const interview::core::InterviewAction& action) {
            next = action;
            completed = true;
        });
    runtime.EndAsyncAudioAnswer();
    runtime.WaitForAudioOperation();
    assert(completed.load());
    assert(next.type == interview::core::InterviewActionType::kAskQuestion);
}

} // namespace

int main() {
    TestScriptedRuntimeFullFlow();
    TestScriptedAudioRuntimeFullFlow();
    TestAsyncAudioRuntimeFlow();
    return 0;
}
