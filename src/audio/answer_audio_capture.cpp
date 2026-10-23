#include "audio/answer_audio_capture.h"

#include <stdexcept>

namespace interview {
namespace audio {

class AnswerAudioCapture::Impl {
public:
    explicit Impl(AudioDevice& device_value) : device(device_value) {}

    void Begin() {
        if (capturing) {
            throw std::runtime_error("answer audio capture is already active");
        }
        bytes.clear();
        capturing = true;
    }

    void CaptureNextChunk() {
        RequireCapturing();
        const auto chunk = device.ReadInput();
        const auto frame_size = device.Config().input_format.BytesPerFrame();
        if (chunk.size() % frame_size != 0) {
            throw std::runtime_error("captured audio is not aligned to input frame size");
        }
        bytes.insert(bytes.end(), chunk.begin(), chunk.end());
    }

    PcmAudio Finish() {
        RequireCapturing();
        capturing = false;
        PcmAudio result;
        result.format = device.Config().input_format;
        result.bytes.assign(bytes.begin(), bytes.end());
        bytes.clear();
        return result;
    }

    void Cancel() {
        bytes.clear();
        capturing = false;
    }

    bool IsCapturing() const { return capturing; }

private:
    void RequireCapturing() const {
        if (!capturing) {
            throw std::runtime_error("answer audio capture is not active");
        }
    }

    AudioDevice& device;
    bool capturing = false;
    std::vector<unsigned char> bytes;
};

AnswerAudioCapture::AnswerAudioCapture(AudioDevice& device)
    : pImpl(std::make_unique<Impl>(device)) {}
AnswerAudioCapture::~AnswerAudioCapture() = default;
AnswerAudioCapture::AnswerAudioCapture(AnswerAudioCapture&&) noexcept = default;
AnswerAudioCapture& AnswerAudioCapture::operator=(AnswerAudioCapture&&) noexcept = default;
void AnswerAudioCapture::Begin() { pImpl->Begin(); }
void AnswerAudioCapture::CaptureNextChunk() { pImpl->CaptureNextChunk(); }
PcmAudio AnswerAudioCapture::Finish() { return pImpl->Finish(); }
void AnswerAudioCapture::Cancel() { pImpl->Cancel(); }
bool AnswerAudioCapture::IsCapturing() const { return pImpl->IsCapturing(); }

} // namespace audio
} // namespace interview
