#include <utility>
#include <stdexcept>

#include "audio/speech_pipeline.h"
#include "audio/answer_audio_capture.h"

namespace interview {
namespace audio {

class SpeechPipeline::Impl {
public:
    Impl(AudioDevice& device_value,
         core::ASRClient& asr_value,
         core::TTSClient& tts_value)
        : device(device_value),
          asr(asr_value),
          tts(tts_value),
          capture(device_value) {}

    void Start() {
        if (started) {
            return;
        }
        device.OpenInput();
        try {
            device.OpenOutput();
        } catch (...) {
            device.Close();
            throw;
        }
        started = true;
    }

    std::string CaptureAnswerText() {
        BeginAnswerCapture();
        CaptureAnswerChunk();
        return FinishAnswerCapture();
    }

    void BeginAnswerCapture() {
        RequireStarted();
        capture.Begin();
    }

    void CaptureAnswerChunk() {
        RequireStarted();
        capture.CaptureNextChunk();
    }

    std::string FinishAnswerCapture() {
        RequireStarted();
        return asr.Transcribe(capture.Finish());
    }

    void CancelAnswerCapture() {
        capture.Cancel();
    }

    void SpeakText(const std::string& text) {
        RequireStarted();
        const auto audio = tts.Synthesize(text);
        const auto output_format = device.Config().output_format;
        if (audio.format.sample_rate_hz != output_format.sample_rate_hz ||
            audio.format.channels != output_format.channels ||
            audio.format.sample_format != output_format.sample_format) {
            throw std::runtime_error("TTS output format does not match audio device output format");
        }
        device.WriteOutput(std::vector<unsigned char>(audio.bytes.begin(), audio.bytes.end()));
    }

    void Stop() {
        capture.Cancel();
        if (!started) {
            return;
        }
        device.Close();
        started = false;
    }

    bool IsStarted() const { return started; }

private:
    void RequireStarted() const {
        if (!started) {
            throw std::runtime_error("speech pipeline is not started");
        }
    }

    AudioDevice& device;
    core::ASRClient& asr;
    core::TTSClient& tts;
    AnswerAudioCapture capture;
    bool started = false;
};

SpeechPipeline::SpeechPipeline(AudioDevice& device,
                               core::ASRClient& asr,
                               core::TTSClient& tts)
    : pImpl(std::make_unique<Impl>(device, asr, tts)) {}

SpeechPipeline::~SpeechPipeline() = default;

SpeechPipeline::SpeechPipeline(SpeechPipeline&&) noexcept = default;

SpeechPipeline& SpeechPipeline::operator=(SpeechPipeline&&) noexcept = default;

void SpeechPipeline::Start() {
    pImpl->Start();
}

std::string SpeechPipeline::CaptureAnswerText() {
    return pImpl->CaptureAnswerText();
}

void SpeechPipeline::BeginAnswerCapture() { pImpl->BeginAnswerCapture(); }
void SpeechPipeline::CaptureAnswerChunk() { pImpl->CaptureAnswerChunk(); }
std::string SpeechPipeline::FinishAnswerCapture() { return pImpl->FinishAnswerCapture(); }
void SpeechPipeline::CancelAnswerCapture() { pImpl->CancelAnswerCapture(); }
bool SpeechPipeline::IsStarted() const { return pImpl->IsStarted(); }

void SpeechPipeline::SpeakText(const std::string& text) {
    pImpl->SpeakText(text);
}

void SpeechPipeline::Stop() {
    pImpl->Stop();
}

} // namespace audio
} // namespace interview
