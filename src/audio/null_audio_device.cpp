#include <deque>
#include <stdexcept>
#include <utility>

#include "audio/null_audio_device.h"

namespace interview {
namespace audio {
namespace {

void ValidateConfig(const AudioDeviceConfig& config) {
    if (config.input_format.BytesPerFrame() == 0 ||
        config.output_format.BytesPerFrame() == 0) {
        throw std::runtime_error("audio device format must have a positive frame size");
    }
    if (config.input_frames_per_buffer <= 0 ||
        config.output_frames_per_buffer <= 0) {
        throw std::runtime_error("audio frames_per_buffer must be positive");
    }
}

} // namespace

class NullAudioDevice::Impl {
public:
    explicit Impl(AudioDeviceConfig config_value)
        : config(std::move(config_value)) {
        ValidateConfig(config);
    }

    void OpenInput() {
        input_open = true;
    }

    void OpenOutput() {
        output_open = true;
    }

    std::vector<unsigned char> ReadInput() {
        if (!input_open) {
            throw std::runtime_error("audio input is not open");
        }

        if (!queued_input.empty()) {
            auto next = std::move(queued_input.front());
            queued_input.pop_front();
            return next;
        }

        return std::vector<unsigned char>(
            static_cast<std::size_t>(config.input_frames_per_buffer) *
            config.input_format.BytesPerFrame(),
            0);
    }

    void WriteOutput(const std::vector<unsigned char>& pcm_audio) {
        if (!output_open) {
            throw std::runtime_error("audio output is not open");
        }
        if (pcm_audio.size() % config.output_format.BytesPerFrame() != 0) {
            throw std::runtime_error("audio output byte length is not aligned to frame size");
        }

        written_output.insert(written_output.end(), pcm_audio.begin(), pcm_audio.end());
    }

    void Close() {
        input_open = false;
        output_open = false;
    }

    AudioDeviceConfig Config() const {
        return config;
    }

    void QueueInput(const std::vector<unsigned char>& pcm_audio) {
        if (pcm_audio.size() % config.input_format.BytesPerFrame() != 0) {
            throw std::runtime_error("audio input byte length is not aligned to frame size");
        }
        queued_input.push_back(pcm_audio);
    }

    std::vector<unsigned char> WrittenOutput() const {
        return written_output;
    }

private:
    AudioDeviceConfig config;
    bool input_open = false;
    bool output_open = false;
    std::deque<std::vector<unsigned char>> queued_input;
    std::vector<unsigned char> written_output;
};

NullAudioDevice::NullAudioDevice(AudioDeviceConfig config)
    : pImpl(std::make_unique<Impl>(std::move(config))) {}

NullAudioDevice::~NullAudioDevice() = default;

NullAudioDevice::NullAudioDevice(NullAudioDevice&&) noexcept = default;

NullAudioDevice& NullAudioDevice::operator=(NullAudioDevice&&) noexcept = default;

void NullAudioDevice::OpenInput() {
    pImpl->OpenInput();
}

void NullAudioDevice::OpenOutput() {
    pImpl->OpenOutput();
}

std::vector<unsigned char> NullAudioDevice::ReadInput() {
    return pImpl->ReadInput();
}

void NullAudioDevice::WriteOutput(const std::vector<unsigned char>& pcm_audio) {
    pImpl->WriteOutput(pcm_audio);
}

void NullAudioDevice::Close() {
    pImpl->Close();
}

AudioDeviceConfig NullAudioDevice::Config() const {
    return pImpl->Config();
}

void NullAudioDevice::QueueInput(const std::vector<unsigned char>& pcm_audio) {
    pImpl->QueueInput(pcm_audio);
}

std::vector<unsigned char> NullAudioDevice::WrittenOutput() const {
    return pImpl->WrittenOutput();
}

} // namespace audio
} // namespace interview
