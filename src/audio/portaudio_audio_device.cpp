#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <portaudio.h>

#include "audio/portaudio_audio_device.h"

namespace interview {
namespace audio {
namespace {

void ValidateConfig(const AudioDeviceConfig& config) {
    if (config.input_format.sample_format != SampleFormat::kPcmS16Le ||
        config.output_format.sample_format != SampleFormat::kPcmS16Le) {
        throw std::runtime_error("PortAudioAudioDevice currently supports only 16-bit little-endian PCM");
    }
    if (config.input_format.sample_rate_hz <= 0 ||
        config.output_format.sample_rate_hz <= 0 ||
        config.input_format.channels <= 0 ||
        config.output_format.channels <= 0 ||
        config.input_frames_per_buffer <= 0 ||
        config.output_frames_per_buffer <= 0) {
        throw std::runtime_error("PortAudioAudioDevice config values must be positive");
    }
}

std::string PortAudioError(const std::string& prefix, PaError error) {
    return prefix + ": " + Pa_GetErrorText(error);
}

PaDeviceIndex FindFirstInputDevice() {
    const PaDeviceIndex count = Pa_GetDeviceCount();
    for (PaDeviceIndex index = 0; index < count; ++index) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(index);
        if (info != nullptr && info->maxInputChannels > 0) {
            return index;
        }
    }
    return paNoDevice;
}

PaDeviceIndex FindFirstOutputDevice() {
    const PaDeviceIndex count = Pa_GetDeviceCount();
    for (PaDeviceIndex index = 0; index < count; ++index) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(index);
        if (info != nullptr && info->maxOutputChannels > 0) {
            return index;
        }
    }
    return paNoDevice;
}

} // namespace

class PortAudioAudioDevice::Impl {
public:
    explicit Impl(AudioDeviceConfig config_value)
        : config(std::move(config_value)) {
        ValidateConfig(config);
        const PaError error = Pa_Initialize();
        if (error != paNoError) {
            throw std::runtime_error(PortAudioError("failed to initialize PortAudio", error));
        }
        initialized = true;
    }

    ~Impl() {
        Close();
    }

    void OpenInput() {
        if (input_stream != nullptr) {
            return;
        }

        PaStreamParameters params{};
        params.device = Pa_GetDefaultInputDevice();
        if (params.device == paNoDevice) {
            params.device = FindFirstInputDevice();
        }
        if (params.device == paNoDevice) {
            throw std::runtime_error("no input audio device found");
        }
        params.channelCount = config.input_format.channels;
        params.sampleFormat = paInt16;
        params.suggestedLatency = Pa_GetDeviceInfo(params.device)->defaultLowInputLatency;
        params.hostApiSpecificStreamInfo = nullptr;

        PaError error = Pa_OpenStream(&input_stream,
                                      &params,
                                      nullptr,
                                      config.input_format.sample_rate_hz,
                                      config.input_frames_per_buffer,
                                      paClipOff,
                                      nullptr,
                                      nullptr);
        if (error != paNoError) {
            throw std::runtime_error(PortAudioError("failed to open input audio stream", error));
        }

        error = Pa_StartStream(input_stream);
        if (error != paNoError) {
            Pa_CloseStream(input_stream);
            input_stream = nullptr;
            throw std::runtime_error(PortAudioError("failed to start input audio stream", error));
        }
    }

    void OpenOutput() {
        if (output_stream != nullptr) {
            return;
        }

        PaStreamParameters params{};
        params.device = Pa_GetDefaultOutputDevice();
        if (params.device == paNoDevice) {
            params.device = FindFirstOutputDevice();
        }
        if (params.device == paNoDevice) {
            throw std::runtime_error("no output audio device found");
        }
        params.channelCount = config.output_format.channels;
        params.sampleFormat = paInt16;
        params.suggestedLatency = Pa_GetDeviceInfo(params.device)->defaultLowOutputLatency;
        params.hostApiSpecificStreamInfo = nullptr;

        PaError error = Pa_OpenStream(&output_stream,
                                      nullptr,
                                      &params,
                                      config.output_format.sample_rate_hz,
                                      config.output_frames_per_buffer,
                                      paClipOff,
                                      nullptr,
                                      nullptr);
        if (error != paNoError) {
            throw std::runtime_error(PortAudioError("failed to open output audio stream", error));
        }

        error = Pa_StartStream(output_stream);
        if (error != paNoError) {
            Pa_CloseStream(output_stream);
            output_stream = nullptr;
            throw std::runtime_error(PortAudioError("failed to start output audio stream", error));
        }
    }

    std::vector<unsigned char> ReadInput() {
        if (input_stream == nullptr) {
            throw std::runtime_error("input audio stream is not open");
        }

        std::vector<unsigned char> buffer(
            static_cast<std::size_t>(config.input_frames_per_buffer) *
            config.input_format.BytesPerFrame());
        const PaError error = Pa_ReadStream(input_stream,
                                            buffer.data(),
                                            config.input_frames_per_buffer);
        if (error != paNoError && error != paInputOverflowed) {
            throw std::runtime_error(PortAudioError("failed to read input audio stream", error));
        }
        return buffer;
    }

    void WriteOutput(const std::vector<unsigned char>& pcm_audio) {
        if (output_stream == nullptr) {
            throw std::runtime_error("output audio stream is not open");
        }
        if (pcm_audio.empty()) {
            return;
        }
        if (pcm_audio.size() % config.output_format.BytesPerFrame() != 0) {
            throw std::runtime_error("output audio byte length is not aligned to frame size");
        }

        const auto frames = static_cast<unsigned long>(
            pcm_audio.size() / config.output_format.BytesPerFrame());
        const PaError error = Pa_WriteStream(output_stream, pcm_audio.data(), frames);
        if (error != paNoError && error != paOutputUnderflowed) {
            throw std::runtime_error(PortAudioError("failed to write output audio stream", error));
        }
    }

    void Close() {
        if (input_stream != nullptr) {
            Pa_StopStream(input_stream);
            Pa_CloseStream(input_stream);
            input_stream = nullptr;
        }
        if (output_stream != nullptr) {
            Pa_StopStream(output_stream);
            Pa_CloseStream(output_stream);
            output_stream = nullptr;
        }
        if (initialized) {
            Pa_Terminate();
            initialized = false;
        }
    }

    AudioDeviceConfig Config() const {
        return config;
    }

private:
    AudioDeviceConfig config;
    PaStream* input_stream = nullptr;
    PaStream* output_stream = nullptr;
    bool initialized = false;
};

PortAudioAudioDevice::PortAudioAudioDevice(AudioDeviceConfig config)
    : pImpl(std::make_unique<Impl>(std::move(config))) {}

PortAudioAudioDevice::~PortAudioAudioDevice() = default;

PortAudioAudioDevice::PortAudioAudioDevice(PortAudioAudioDevice&&) noexcept = default;

PortAudioAudioDevice& PortAudioAudioDevice::operator=(PortAudioAudioDevice&&) noexcept = default;

void PortAudioAudioDevice::OpenInput() {
    pImpl->OpenInput();
}

void PortAudioAudioDevice::OpenOutput() {
    pImpl->OpenOutput();
}

std::vector<unsigned char> PortAudioAudioDevice::ReadInput() {
    return pImpl->ReadInput();
}

void PortAudioAudioDevice::WriteOutput(const std::vector<unsigned char>& pcm_audio) {
    pImpl->WriteOutput(pcm_audio);
}

void PortAudioAudioDevice::Close() {
    pImpl->Close();
}

AudioDeviceConfig PortAudioAudioDevice::Config() const {
    return pImpl->Config();
}

} // namespace audio
} // namespace interview
