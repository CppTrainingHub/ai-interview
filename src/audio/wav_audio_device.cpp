#include <stdexcept>
#include <utility>

#include "audio/wav_audio_device.h"
#include "audio/wav_file.h"

namespace interview {
namespace audio {
namespace {

void ValidateConfig(const AudioDeviceConfig& config) {
    if (config.input_format.BytesPerFrame() == 0 ||
        config.output_format.BytesPerFrame() == 0) {
        throw std::runtime_error("wav audio device format must have a positive frame size");
    }
    if (config.input_frames_per_buffer <= 0 ||
        config.output_frames_per_buffer <= 0) {
        throw std::runtime_error("wav audio frames_per_buffer must be positive");
    }
}

} // namespace

class WavAudioDevice::Impl {
public:
    Impl(std::string input_path_value,
         std::string output_path_value,
         AudioDeviceConfig config_value)
        : input_path(std::move(input_path_value)),
          output_path(std::move(output_path_value)),
          config(std::move(config_value)) {
        ValidateConfig(config);
    }

    void OpenInput() {
        if (!input_path.empty()) {
            const auto audio = ReadWavFile(input_path);
            if (audio.format.sample_rate_hz != config.input_format.sample_rate_hz ||
                audio.format.channels != config.input_format.channels ||
                audio.format.sample_format != config.input_format.sample_format) {
                throw std::runtime_error("input wav format does not match audio device input format");
            }
            input_bytes.assign(audio.bytes.begin(), audio.bytes.end());
        }
        input_offset = 0;
        input_open = true;
    }

    void OpenOutput() {
        output_bytes.clear();
        output_open = true;
    }

    std::vector<unsigned char> ReadInput() {
        if (!input_open) {
            throw std::runtime_error("wav audio input is not open");
        }

        const auto chunk_size =
            static_cast<std::size_t>(config.input_frames_per_buffer) *
            config.input_format.BytesPerFrame();
        if (input_offset >= input_bytes.size()) {
            return std::vector<unsigned char>(chunk_size, 0);
        }

        const auto remaining = input_bytes.size() - input_offset;
        const auto bytes_to_read = remaining < chunk_size ? remaining : chunk_size;
        std::vector<unsigned char> chunk(
            input_bytes.begin() + static_cast<std::ptrdiff_t>(input_offset),
            input_bytes.begin() + static_cast<std::ptrdiff_t>(input_offset + bytes_to_read));
        input_offset += bytes_to_read;

        if (chunk.size() < chunk_size) {
            chunk.resize(chunk_size, 0);
        }

        return chunk;
    }

    void WriteOutput(const std::vector<unsigned char>& pcm_audio) {
        if (!output_open) {
            throw std::runtime_error("wav audio output is not open");
        }
        if (pcm_audio.size() % config.output_format.BytesPerFrame() != 0) {
            throw std::runtime_error("wav audio output byte length is not aligned to frame size");
        }
        output_bytes.insert(output_bytes.end(), pcm_audio.begin(), pcm_audio.end());
    }

    void Close() {
        if (output_open && !output_path.empty()) {
            WriteWavFile(output_path, output_bytes, config.output_format);
        }
        input_open = false;
        output_open = false;
    }

    AudioDeviceConfig Config() const {
        return config;
    }

    std::vector<unsigned char> WrittenOutput() const {
        return output_bytes;
    }

private:
    std::string input_path;
    std::string output_path;
    AudioDeviceConfig config;
    bool input_open = false;
    bool output_open = false;
    std::vector<unsigned char> input_bytes;
    std::size_t input_offset = 0;
    std::vector<unsigned char> output_bytes;
};

WavAudioDevice::WavAudioDevice(std::string input_path,
                               std::string output_path,
                               AudioDeviceConfig config)
    : pImpl(std::make_unique<Impl>(
          std::move(input_path),
          std::move(output_path),
          std::move(config))) {}

WavAudioDevice::~WavAudioDevice() = default;

WavAudioDevice::WavAudioDevice(WavAudioDevice&&) noexcept = default;

WavAudioDevice& WavAudioDevice::operator=(WavAudioDevice&&) noexcept = default;

void WavAudioDevice::OpenInput() {
    pImpl->OpenInput();
}

void WavAudioDevice::OpenOutput() {
    pImpl->OpenOutput();
}

std::vector<unsigned char> WavAudioDevice::ReadInput() {
    return pImpl->ReadInput();
}

void WavAudioDevice::WriteOutput(const std::vector<unsigned char>& pcm_audio) {
    pImpl->WriteOutput(pcm_audio);
}

void WavAudioDevice::Close() {
    pImpl->Close();
}

AudioDeviceConfig WavAudioDevice::Config() const {
    return pImpl->Config();
}

std::vector<unsigned char> WavAudioDevice::WrittenOutput() const {
    return pImpl->WrittenOutput();
}

} // namespace audio
} // namespace interview
