#include "services/audio_manager.h"
#include "common/logger.h"
#include <portaudio.h>
#include <stdexcept>
#include <cstring>

namespace interview {
namespace services {

class AudioDeviceManager::AudioManagerImpl {
public:
    AudioManagerImpl(const AudioConfig& input_config, const AudioConfig& output_config)
        : input_config_(input_config)
        , output_config_(output_config)
        , input_stream_(nullptr)
        , output_stream_(nullptr)
        , pa_initialized_(false) {

        // 初始化 PortAudio
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            throw std::runtime_error(std::string("Failed to initialize PortAudio: ") + Pa_GetErrorText(err));
        }
        pa_initialized_ = true;
        LOG_INFO("PortAudio initialized successfully");
    }

    ~AudioManagerImpl() {
        Cleanup();
    }

    void OpenInputStream() {
        if (input_stream_) {
            LOG_WARNING("Input stream already opened");
            return;
        }

        PaStreamParameters input_parameters;
        input_parameters.device = Pa_GetDefaultInputDevice();
        if (input_parameters.device == paNoDevice) {
            throw std::runtime_error("No default input device found");
        }

        input_parameters.channelCount = input_config_.channels;
        input_parameters.sampleFormat = paInt16;  // 16-bit PCM
        input_parameters.suggestedLatency = Pa_GetDeviceInfo(input_parameters.device)->defaultLowInputLatency;
        input_parameters.hostApiSpecificStreamInfo = nullptr;

        PaError err = Pa_OpenStream(
            &input_stream_,
            &input_parameters,
            nullptr,  // 无输出
            input_config_.sample_rate,
            input_config_.chunk,
            paClipOff,
            nullptr,  // 无回调，使用阻塞模式
            nullptr
        );

        if (err != paNoError) {
            throw std::runtime_error(std::string("Failed to open input stream: ") + Pa_GetErrorText(err));
        }

        err = Pa_StartStream(input_stream_);
        if (err != paNoError) {
            Pa_CloseStream(input_stream_);
            input_stream_ = nullptr;
            throw std::runtime_error(std::string("Failed to start input stream: ") + Pa_GetErrorText(err));
        }

        LOG_INFO("Input stream opened: ", input_config_.sample_rate, "Hz, ",
                 input_config_.channels, " channel(s), ",
                 input_config_.chunk, " frames/buffer");
    }

    void OpenOutputStream() {
        if (output_stream_) {
            LOG_WARNING("Output stream already opened");
            return;
        }

        PaStreamParameters output_parameters;
        output_parameters.device = Pa_GetDefaultOutputDevice();
        if (output_parameters.device == paNoDevice) {
            throw std::runtime_error("No default output device found");
        }

        output_parameters.channelCount = output_config_.channels;
        output_parameters.sampleFormat = paFloat32;  // 32-bit float
        output_parameters.suggestedLatency = Pa_GetDeviceInfo(output_parameters.device)->defaultLowOutputLatency;
        output_parameters.hostApiSpecificStreamInfo = nullptr;

        PaError err = Pa_OpenStream(
            &output_stream_,
            nullptr,  // 无输入
            &output_parameters,
            output_config_.sample_rate,
            output_config_.chunk,
            paClipOff,
            nullptr,
            nullptr
        );

        if (err != paNoError) {
            throw std::runtime_error(std::string("Failed to open output stream: ") + Pa_GetErrorText(err));
        }

        err = Pa_StartStream(output_stream_);
        if (err != paNoError) {
            Pa_CloseStream(output_stream_);
            output_stream_ = nullptr;
            throw std::runtime_error(std::string("Failed to start output stream: ") + Pa_GetErrorText(err));
        }

        LOG_INFO("Output stream opened: ", output_config_.sample_rate, "Hz, ",
                 output_config_.channels, " channel(s), ",
                 output_config_.chunk, " frames/buffer");
    }

    std::vector<int16_t> ReadAudio() {
        if (!input_stream_) {
            throw std::runtime_error("Input stream not opened");
        }

        std::vector<int16_t> buffer(input_config_.chunk * input_config_.channels);

        PaError err = Pa_ReadStream(input_stream_, buffer.data(), input_config_.chunk);

        if (err == paInputOverflowed) {
            LOG_WARNING("Input overflow detected (buffer overrun)");
            // 继续返回数据，但记录警告
        } else if (err != paNoError) {
            throw std::runtime_error(std::string("Failed to read audio: ") + Pa_GetErrorText(err));
        }

        return buffer;
    }

    void WriteAudio(const std::vector<float>& audio) {
        if (!output_stream_) {
            throw std::runtime_error("Output stream not opened");
        }

        if (audio.empty()) {
            return;
        }

        PaError err = Pa_WriteStream(output_stream_, audio.data(),
                                     audio.size() / output_config_.channels);

        if (err == paOutputUnderflowed) {
            LOG_WARNING("Output underflow detected");
        } else if (err != paNoError) {
            throw std::runtime_error(std::string("Failed to write audio: ") + Pa_GetErrorText(err));
        }
    }

    void Cleanup() {
        // 关闭输入流（带错误处理）
        if (input_stream_) {
            PaError err = Pa_StopStream(input_stream_);
            if (err != paNoError) {
                LOG_WARNING("Failed to stop input stream: {}", Pa_GetErrorText(err));
            }
            err = Pa_CloseStream(input_stream_);
            if (err != paNoError) {
                LOG_WARNING("Failed to close input stream: {}", Pa_GetErrorText(err));
            }
            input_stream_ = nullptr;
            LOG_INFO("Input stream closed");
        }

        // 关闭输出流（带错误处理）
        if (output_stream_) {
            PaError err = Pa_StopStream(output_stream_);
            if (err != paNoError) {
                LOG_WARNING("Failed to stop output stream: {}", Pa_GetErrorText(err));
            }
            err = Pa_CloseStream(output_stream_);
            if (err != paNoError) {
                LOG_WARNING("Failed to close output stream: {}", Pa_GetErrorText(err));
            }
            output_stream_ = nullptr;
            LOG_INFO("Output stream closed");
        }

        // 终止 PortAudio（带错误处理）
        if (pa_initialized_) {
            PaError err = Pa_Terminate();
            if (err != paNoError) {
                LOG_WARNING("Failed to terminate PortAudio: {}", Pa_GetErrorText(err));
            }
            pa_initialized_ = false;
            LOG_INFO("PortAudio terminated");
        }
    }

private:
    AudioConfig input_config_;
    AudioConfig output_config_;
    PaStream* input_stream_;
    PaStream* output_stream_;
    bool pa_initialized_;
};

AudioDeviceManager::AudioDeviceManager(const AudioConfig& input_config,
                                       const AudioConfig& output_config)
    : pimpl_(std::make_unique<AudioManagerImpl>(input_config, output_config)) {
}

AudioDeviceManager::~AudioDeviceManager() = default;

void AudioDeviceManager::OpenInputStream() {
    pimpl_->OpenInputStream();
}

void AudioDeviceManager::OpenOutputStream() {
    pimpl_->OpenOutputStream();
}

std::vector<int16_t> AudioDeviceManager::ReadAudio() {
    return pimpl_->ReadAudio();
}

void AudioDeviceManager::WriteAudio(const std::vector<float>& audio) {
    pimpl_->WriteAudio(audio);
}

void AudioDeviceManager::Cleanup() {
    pimpl_->Cleanup();
}

} // namespace services
} // namespace interview
