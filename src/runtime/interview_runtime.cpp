#include "runtime/interview_runtime.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "audio/null_audio_device.h"
#include "audio/async_speech_controller.h"
#include "audio/scripted_asr_client.h"
#include "audio/speech_pipeline.h"
#include "audio/text_to_pcm_tts_client.h"
#include "audio/volcengine_realtime_speech_client.h"
#include "audio/wav_audio_device.h"
#include "core/interview_orchestrator.h"
#include "core/interview_ports.h"
#include "core/interview_session.h"
#include "core/report_builder.h"
#include "llm/llm_answer_evaluator.h"
#include "llm/llm_client.h"
#include "llm/llm_question_generator.h"
#include "llm/llm_summary_generator.h"
#include "pdf/pdf_resume_parser.h"
#include "report/json_report_writer.h"

#if defined(AI_INTERVIEW_ENABLE_PORTAUDIO)
#include "audio/portaudio_audio_device.h"
#endif

namespace interview {
namespace runtime {
namespace {

std::string SafeFilePart(std::string value) {
    if (value.empty()) {
        return "Candidate";
    }
    for (char& ch : value) {
        if (ch == ' ' || ch == '/' || ch == '\\') {
            ch = '_';
        }
    }
    return value;
}

std::vector<std::string> SplitTranscripts(const std::string& value) {
    if (value.empty()) {
        return {
            "I am a C++ candidate with project experience in backend systems.",
            "RAII binds resources to object lifetime and releases them in destructors.",
            "std::vector stores contiguous elements and grows by reallocating capacity.",
            "I would protect shared state with mutexes and minimize critical sections."
        };
    }

    std::vector<std::string> result;
    std::size_t offset = 0;
    while (offset <= value.size()) {
        const auto separator = value.find("||", offset);
        const auto end = separator == std::string::npos ? value.size() : separator;
        const auto item = value.substr(offset, end - offset);
        if (!item.empty()) {
            result.push_back(item);
        }
        if (separator == std::string::npos) {
            break;
        }
        offset = separator + 2;
    }
    return result;
}

class ScriptedQuestionGenerator final : public core::QuestionGenerator {
public:
    std::vector<core::InterviewQuestion> GenerateFromResume(
        const std::string& resume_text,
        int min_questions) override {
        auto questions = GenerateDefault(min_questions);
        if (!resume_text.empty()) {
            questions.front().question =
                "请结合你的简历项目，说明一个你最熟悉的 C++ 技术决策。";
            questions.front().category = "resume_project";
        }
        return questions;
    }

    std::vector<core::InterviewQuestion> GenerateDefault(int min_questions) override {
        if (min_questions <= 0) {
            throw std::runtime_error("min_questions must be positive");
        }

        std::vector<core::InterviewQuestion> seed = {
            MakeQuestion("请解释 RAII 的含义，并举一个实际使用场景。", "cpp_basic", {"RAII", "析构"}),
            MakeQuestion("std::vector 扩容时会发生什么？这对迭代器有什么影响？", "stl", {"vector", "capacity"}),
            MakeQuestion("多线程中如何避免数据竞争？请说出一种 C++ 标准库方案。", "concurrency", {"mutex", "lock"}),
            MakeQuestion("如果一个接口偶发超时，你会如何定位问题？", "engineering", {"log", "timeout"})
        };

        while (static_cast<int>(seed.size()) < min_questions) {
            seed.push_back(MakeQuestion(
                "请描述一个你解决过的技术问题，以及你如何验证修复结果。",
                "project",
                {"debug", "test"}));
        }
        seed.resize(static_cast<std::size_t>(min_questions));
        return seed;
    }

private:
    static core::InterviewQuestion MakeQuestion(
        std::string text,
        std::string category,
        std::vector<std::string> keywords) {
        core::InterviewQuestion question;
        question.question = std::move(text);
        question.category = std::move(category);
        question.level = "intermediate";
        question.expected_keywords = std::move(keywords);
        question.key_points = question.expected_keywords;
        return question;
    }
};

class ScriptedAnswerEvaluator final : public core::AnswerEvaluator {
public:
    core::AnswerEvaluation Evaluate(const core::InterviewQuestion& question,
                                    const std::string& answer,
                                    bool is_followup) override {
        core::AnswerEvaluation evaluation;
        evaluation.score = answer.size() < 20U ? 55 : 82;
        evaluation.feedback = "离线评分：回答已记录，关键点覆盖情况可在真实 LLM 模式下细化。";
        evaluation.need_followup = !is_followup && answer.size() < 60U;
        if (evaluation.need_followup) {
            evaluation.followup_question =
                "请再补充一个和“" + question.category + "”相关的具体例子。";
        }
        return evaluation;
    }
};

class ScriptedSummaryGenerator final : public core::SummaryGenerator {
public:
    std::string GenerateSummary(const std::vector<core::InterviewRecord>& records,
                                const std::string&) override {
        int total_score = 0;
        for (const auto& record : records) {
            total_score += record.evaluation.score;
        }
        const int average = records.empty()
            ? 0
            : total_score / static_cast<int>(records.size());

        return "离线面试总结：候选人完成了 " +
               std::to_string(records.size()) +
               " 条回答，平均分约为 " +
               std::to_string(average) +
               "。该总结用于无外部服务的项目闭环验证。";
    }
};

class PlainTextResumeParser final : public core::ResumeParser {
public:
    std::string ExtractText(const std::string& resume_path) override {
        if (resume_path.empty()) {
            return "";
        }
        return "离线简历占位文本：" + resume_path;
    }
};

std::unique_ptr<audio::AudioDevice> CreateAudioDevice(const config::AppConfig& config) {
    audio::AudioDeviceConfig audio_config;
    audio_config.input_format.sample_rate_hz = config.audio_input_sample_rate_hz;
    audio_config.output_format.sample_rate_hz = config.audio_output_sample_rate_hz;
    audio_config.input_frames_per_buffer = config.audio_input_frames_per_buffer;
    audio_config.output_frames_per_buffer = config.audio_output_frames_per_buffer;

    if (config.audio_device == "null") {
        return std::make_unique<audio::NullAudioDevice>(audio_config);
    }
    if (config.audio_device == "wav") {
        if (config.input_wav_path.empty()) {
            throw std::runtime_error("AI_INTERVIEW_INPUT_WAV_PATH is required for wav audio");
        }
        return std::make_unique<audio::WavAudioDevice>(
            config.input_wav_path,
            config.output_wav_path,
            audio_config);
    }
    if (config.audio_device == "portaudio") {
#if defined(AI_INTERVIEW_ENABLE_PORTAUDIO)
        return std::make_unique<audio::PortAudioAudioDevice>(audio_config);
#else
        throw std::runtime_error("portaudio device requires AI_INTERVIEW_ENABLE_PORTAUDIO");
#endif
    }

    throw std::runtime_error("unsupported audio device: " + config.audio_device);
}

audio::VolcengineRealtimeSpeechConfig CreateVolcengineConfig(
    const config::AppConfig& config) {
    if (config.volcengine_app_id.empty() || config.volcengine_access_key.empty() ||
        config.volcengine_app_key.empty()) {
        throw std::runtime_error("Volcengine speech credentials are incomplete");
    }
    audio::VolcengineRealtimeSpeechConfig speech;
    speech.websocket_url = config.volcengine_websocket_url;
    speech.headers["X-Api-App-ID"] = config.volcengine_app_id;
    speech.headers["X-Api-Access-Key"] = config.volcengine_access_key;
    speech.headers["X-Api-Resource-Id"] = config.volcengine_resource_id;
    speech.headers["X-Api-App-Key"] = config.volcengine_app_key;
    speech.input_format.sample_rate_hz = config.audio_input_sample_rate_hz;
    speech.output_format.sample_rate_hz = config.audio_output_sample_rate_hz;
    speech.speaker = config.volcengine_speaker;
    speech.operation_timeout_ms = config.volcengine_operation_timeout_ms;
    return speech;
}

} // namespace

class InterviewRuntime::Impl {
public:
    Impl(config::AppConfig config_value, std::string candidate)
        : config(std::move(config_value)),
          candidate_name(std::move(candidate)),
          session(candidate_name.empty() ? "Candidate" : candidate_name) {
        CreateBusinessPorts();
        CreateOrchestrator();
        CreateSpeechPipelineIfNeeded();
    }

    ~Impl() {
        if (async_speech) {
            async_speech->RequestStopCapture();
            async_speech->Wait();
        }
        if (speech_pipeline && speech_started) {
            try {
                speech_pipeline->Stop();
            } catch (...) {
            }
        }
    }

    core::InterviewAction Start(const std::string& resume_path, int min_questions) {
        if (min_questions <= 0) {
            throw std::runtime_error("min_questions must be positive");
        }
        if (resume_path.empty()) {
            orchestrator->ConfigureDefaultInterview(min_questions);
        } else {
            orchestrator->ConfigureResumeInterview(resume_path, min_questions);
        }

        if (speech_pipeline) {
            speech_pipeline->Start();
            speech_started = true;
        }
        return orchestrator->Start();
    }

    core::InterviewAction SubmitTextAnswer(const std::string& answer) {
        if (answer.empty()) {
            throw std::runtime_error("answer must not be empty");
        }
        auto action = orchestrator->ProcessCandidateAnswer(answer);
        if (action.type == core::InterviewActionType::kFinish) {
            last_summary = action.text;
        }
        return action;
    }

    core::InterviewAction CaptureAndSubmitAudioAnswer() {
        if (!speech_pipeline) {
            throw std::runtime_error("audio runtime is not enabled");
        }
        return SubmitTextAnswer(speech_pipeline->CaptureAnswerText());
    }

    void BeginAsyncAudioAnswer(
        std::function<void(const core::InterviewAction&)> on_action,
        std::function<void(const std::string&)> on_error,
        std::function<void(const std::string&)> on_transcript) {
        RequireAsyncSpeech().StartCapture(
            [this,
             callback = std::move(on_action),
             transcript = std::move(on_transcript)](const std::string& text) {
                try {
                    if (transcript) {
                        transcript(text);
                    }
                    const auto action = SubmitTextAnswer(text);
                    if (callback) {
                        callback(action);
                    }
                } catch (const std::exception& exception) {
                    throw std::runtime_error(exception.what());
                }
            },
            std::move(on_error));
    }

    void EndAsyncAudioAnswer() { RequireAsyncSpeech().RequestStopCapture(); }

    void SpeakActionAsync(const core::InterviewAction& action,
                          std::function<void()> on_complete,
                          std::function<void(const std::string&)> on_error) {
        if (action.text.empty()) {
            if (on_complete) {
                on_complete();
            }
            return;
        }
        RequireAsyncSpeech().Speak(action.text, std::move(on_complete), std::move(on_error));
    }

    void WaitForAudioOperation() { RequireAsyncSpeech().Wait(); }

    void SpeakAction(const core::InterviewAction& action) {
        if (speech_pipeline && !action.text.empty()) {
            speech_pipeline->SpeakText(action.text);
        }
    }

    std::string GenerateReport(const std::string& output_suffix) {
        core::ReportBuilder builder;
        const auto report_data = builder.BuildReport(
            session,
            last_summary,
            std::time(nullptr));

        std::filesystem::create_directories(config.report_output_dir);
        const std::string suffix = output_suffix.empty() ? "cli" : output_suffix;
        const auto path = std::filesystem::path(config.report_output_dir) /
            (SafeFilePart(candidate_name) + "_" + SafeFilePart(suffix) + "_interview_report.json");
        return report_writer.WriteReport(report_data, path.string());
    }

private:
    void CreateBusinessPorts() {
        if (config.dependency_mode == "scripted") {
            scripted_resume_parser = std::make_unique<PlainTextResumeParser>();
            scripted_question_generator = std::make_unique<ScriptedQuestionGenerator>();
            scripted_answer_evaluator = std::make_unique<ScriptedAnswerEvaluator>();
            scripted_summary_generator = std::make_unique<ScriptedSummaryGenerator>();

            resume_parser = scripted_resume_parser.get();
            question_generator = scripted_question_generator.get();
            answer_evaluator = scripted_answer_evaluator.get();
            summary_generator = scripted_summary_generator.get();
            return;
        }

        llm_client = std::make_unique<llm::OpenAICompatibleLLMClient>(config.ToLLMConfig());
        llm_question_generator = std::make_unique<llm::LLMQuestionGenerator>(*llm_client);
        llm_answer_evaluator = std::make_unique<llm::LLMAnswerEvaluator>(*llm_client);
        llm_summary_generator = std::make_unique<llm::LLMSummaryGenerator>(*llm_client);
        pdf_resume_parser = std::make_unique<pdf::PdfResumeParser>();

        resume_parser = pdf_resume_parser.get();
        question_generator = llm_question_generator.get();
        answer_evaluator = llm_answer_evaluator.get();
        summary_generator = llm_summary_generator.get();
    }

    void CreateOrchestrator() {
        orchestrator = std::make_unique<core::InterviewOrchestrator>(
            session,
            *resume_parser,
            *question_generator,
            *answer_evaluator,
            *summary_generator);
    }

    void CreateSpeechPipelineIfNeeded() {
        if (config.runtime_mode != "audio") {
            return;
        }

        audio_device = CreateAudioDevice(config);

        if (config.asr_provider == "scripted") {
            scripted_asr = std::make_unique<audio::ScriptedASRClient>(
                SplitTranscripts(config.scripted_transcripts));
            asr = scripted_asr.get();
        } else if (config.asr_provider == "volcengine") {
            realtime_speech = std::make_unique<audio::VolcengineRealtimeSpeechClient>(
                CreateVolcengineConfig(config));
            asr = realtime_speech.get();
        } else {
            throw std::runtime_error("unsupported ASR provider: " + config.asr_provider);
        }

        if (config.tts_provider == "silent_pcm") {
            audio::AudioFormat output_format;
            output_format.sample_rate_hz = config.audio_output_sample_rate_hz;
            silent_tts = std::make_unique<audio::TextToPcmTTSClient>(output_format);
            tts = silent_tts.get();
        } else if (config.tts_provider == "volcengine") {
            if (!realtime_speech) {
                realtime_speech = std::make_unique<audio::VolcengineRealtimeSpeechClient>(
                    CreateVolcengineConfig(config));
            }
            tts = realtime_speech.get();
        } else {
            throw std::runtime_error("unsupported TTS provider: " + config.tts_provider);
        }

        speech_pipeline = std::make_unique<audio::SpeechPipeline>(*audio_device, *asr, *tts);
        async_speech = std::make_unique<audio::AsyncSpeechController>(*speech_pipeline);
    }

    audio::AsyncSpeechController& RequireAsyncSpeech() {
        if (!async_speech) {
            throw std::runtime_error("audio runtime is not enabled");
        }
        return *async_speech;
    }

    config::AppConfig config;
    std::string candidate_name;
    std::string last_summary;

    core::InterviewSession session;
    report::JsonReportWriter report_writer;

    core::ResumeParser* resume_parser = nullptr;
    core::QuestionGenerator* question_generator = nullptr;
    core::AnswerEvaluator* answer_evaluator = nullptr;
    core::SummaryGenerator* summary_generator = nullptr;

    std::unique_ptr<PlainTextResumeParser> scripted_resume_parser;
    std::unique_ptr<ScriptedQuestionGenerator> scripted_question_generator;
    std::unique_ptr<ScriptedAnswerEvaluator> scripted_answer_evaluator;
    std::unique_ptr<ScriptedSummaryGenerator> scripted_summary_generator;

    std::unique_ptr<llm::OpenAICompatibleLLMClient> llm_client;
    std::unique_ptr<llm::LLMQuestionGenerator> llm_question_generator;
    std::unique_ptr<llm::LLMAnswerEvaluator> llm_answer_evaluator;
    std::unique_ptr<llm::LLMSummaryGenerator> llm_summary_generator;
    std::unique_ptr<pdf::PdfResumeParser> pdf_resume_parser;

    core::ASRClient* asr = nullptr;
    core::TTSClient* tts = nullptr;
    std::unique_ptr<audio::AudioDevice> audio_device;
    std::unique_ptr<audio::ScriptedASRClient> scripted_asr;
    std::unique_ptr<audio::TextToPcmTTSClient> silent_tts;
    std::unique_ptr<audio::VolcengineRealtimeSpeechClient> realtime_speech;
    std::unique_ptr<audio::SpeechPipeline> speech_pipeline;
    std::unique_ptr<audio::AsyncSpeechController> async_speech;
    bool speech_started = false;

    std::unique_ptr<core::InterviewOrchestrator> orchestrator;
};

InterviewRuntime::InterviewRuntime(config::AppConfig config, std::string candidate_name)
    : pImpl(std::make_unique<Impl>(std::move(config), std::move(candidate_name))) {}

InterviewRuntime::~InterviewRuntime() = default;

InterviewRuntime::InterviewRuntime(InterviewRuntime&&) noexcept = default;

InterviewRuntime& InterviewRuntime::operator=(InterviewRuntime&&) noexcept = default;

core::InterviewAction InterviewRuntime::Start(const std::string& resume_path,
                                              int min_questions) {
    return pImpl->Start(resume_path, min_questions);
}

core::InterviewAction InterviewRuntime::SubmitTextAnswer(const std::string& answer) {
    return pImpl->SubmitTextAnswer(answer);
}

core::InterviewAction InterviewRuntime::CaptureAndSubmitAudioAnswer() {
    return pImpl->CaptureAndSubmitAudioAnswer();
}

void InterviewRuntime::BeginAsyncAudioAnswer(
    std::function<void(const core::InterviewAction&)> on_action,
    std::function<void(const std::string&)> on_error,
    std::function<void(const std::string&)> on_transcript) {
    pImpl->BeginAsyncAudioAnswer(
        std::move(on_action), std::move(on_error), std::move(on_transcript));
}

void InterviewRuntime::EndAsyncAudioAnswer() { pImpl->EndAsyncAudioAnswer(); }

void InterviewRuntime::SpeakActionAsync(
    const core::InterviewAction& action,
    std::function<void()> on_complete,
    std::function<void(const std::string&)> on_error) {
    pImpl->SpeakActionAsync(action, std::move(on_complete), std::move(on_error));
}

void InterviewRuntime::WaitForAudioOperation() { pImpl->WaitForAudioOperation(); }

void InterviewRuntime::SpeakAction(const core::InterviewAction& action) {
    pImpl->SpeakAction(action);
}

std::string InterviewRuntime::GenerateReport(const std::string& output_suffix) {
    return pImpl->GenerateReport(output_suffix);
}

} // namespace runtime
} // namespace interview
