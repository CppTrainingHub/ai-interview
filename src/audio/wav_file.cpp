#include <array>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "audio/wav_file.h"

namespace interview {
namespace audio {
namespace {

constexpr std::uint16_t kPcmAudioFormat = 1;
constexpr std::uint16_t kPcmS16BitsPerSample = 16;

void ValidatePcmS16Format(const AudioFormat& format) {
    if (format.sample_rate_hz <= 0) {
        throw std::runtime_error("audio sample_rate_hz must be positive");
    }
    if (format.channels <= 0) {
        throw std::runtime_error("audio channels must be positive");
    }
    if (format.sample_format != SampleFormat::kPcmS16Le) {
        throw std::runtime_error("only 16-bit little-endian PCM wav files are supported");
    }
}

void WriteAscii(std::ostream& output, const char* value, std::size_t size) {
    output.write(value, static_cast<std::streamsize>(size));
}

void WriteU16Le(std::ostream& output, std::uint16_t value) {
    const std::array<unsigned char, 2> bytes{{
        static_cast<unsigned char>(value & 0xFFU),
        static_cast<unsigned char>((value >> 8U) & 0xFFU)
    }};
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void WriteU32Le(std::ostream& output, std::uint32_t value) {
    const std::array<unsigned char, 4> bytes{{
        static_cast<unsigned char>(value & 0xFFU),
        static_cast<unsigned char>((value >> 8U) & 0xFFU),
        static_cast<unsigned char>((value >> 16U) & 0xFFU),
        static_cast<unsigned char>((value >> 24U) & 0xFFU)
    }};
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

std::uint16_t ReadU16Le(std::istream& input) {
    std::array<unsigned char, 2> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!input) {
        throw std::runtime_error("unexpected end of wav file");
    }
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(bytes[1] << 8U);
}

std::uint32_t ReadU32Le(std::istream& input) {
    std::array<unsigned char, 4> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!input) {
        throw std::runtime_error("unexpected end of wav file");
    }
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

std::string ReadAscii(std::istream& input, std::size_t size) {
    std::string value(size, '\0');
    input.read(&value[0], static_cast<std::streamsize>(size));
    if (!input) {
        throw std::runtime_error("unexpected end of wav file");
    }
    return value;
}

void SkipBytes(std::istream& input, std::uint32_t size) {
    input.seekg(static_cast<std::streamoff>(size), std::ios::cur);
    if (!input) {
        throw std::runtime_error("failed to skip wav chunk");
    }
}

} // namespace

void WriteWavFile(const std::string& output_path,
                  const std::vector<unsigned char>& pcm_audio,
                  const AudioFormat& format) {
    ValidatePcmS16Format(format);

    const auto bytes_per_frame = format.BytesPerFrame();
    if (bytes_per_frame == 0 || pcm_audio.size() % bytes_per_frame != 0) {
        throw std::runtime_error("pcm audio byte length is not aligned to frame size");
    }

    const auto data_size = static_cast<std::uint32_t>(pcm_audio.size());
    const auto fmt_chunk_size = static_cast<std::uint32_t>(16);
    const auto riff_size = static_cast<std::uint32_t>(4 + 8 + fmt_chunk_size + 8 + data_size);
    const auto byte_rate = static_cast<std::uint32_t>(
        format.sample_rate_hz * static_cast<int>(format.BytesPerFrame())
    );
    const auto block_align = static_cast<std::uint16_t>(format.BytesPerFrame());

    std::ofstream output(output_path, std::ios::binary);
    if (!output.is_open()) {
        throw std::runtime_error("failed to open wav output file: " + output_path);
    }

    WriteAscii(output, "RIFF", 4);
    WriteU32Le(output, riff_size);
    WriteAscii(output, "WAVE", 4);
    WriteAscii(output, "fmt ", 4);
    WriteU32Le(output, fmt_chunk_size);
    WriteU16Le(output, kPcmAudioFormat);
    WriteU16Le(output, static_cast<std::uint16_t>(format.channels));
    WriteU32Le(output, static_cast<std::uint32_t>(format.sample_rate_hz));
    WriteU32Le(output, byte_rate);
    WriteU16Le(output, block_align);
    WriteU16Le(output, kPcmS16BitsPerSample);
    WriteAscii(output, "data", 4);
    WriteU32Le(output, data_size);
    output.write(reinterpret_cast<const char*>(pcm_audio.data()),
                 static_cast<std::streamsize>(pcm_audio.size()));

    if (!output) {
        throw std::runtime_error("failed to write wav output file: " + output_path);
    }
}

PcmAudio ReadWavFile(const std::string& input_path) {
    std::ifstream input(input_path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open wav input file: " + input_path);
    }

    if (ReadAscii(input, 4) != "RIFF") {
        throw std::runtime_error("wav file missing RIFF header");
    }
    ReadU32Le(input);
    if (ReadAscii(input, 4) != "WAVE") {
        throw std::runtime_error("wav file missing WAVE header");
    }

    AudioFormat format;
    bool found_fmt = false;
    bool found_data = false;
    std::vector<unsigned char> data;

    while (input && !(found_fmt && found_data)) {
        const std::string chunk_id = ReadAscii(input, 4);
        const std::uint32_t chunk_size = ReadU32Le(input);

        if (chunk_id == "fmt ") {
            const auto audio_format = ReadU16Le(input);
            const auto channels = ReadU16Le(input);
            const auto sample_rate = ReadU32Le(input);
            ReadU32Le(input);
            ReadU16Le(input);
            const auto bits_per_sample = ReadU16Le(input);

            if (chunk_size > 16U) {
                SkipBytes(input, chunk_size - 16U);
            }

            if (audio_format != kPcmAudioFormat || bits_per_sample != kPcmS16BitsPerSample) {
                throw std::runtime_error("only 16-bit PCM wav files are supported");
            }

            format.channels = static_cast<int>(channels);
            format.sample_rate_hz = static_cast<int>(sample_rate);
            format.sample_format = SampleFormat::kPcmS16Le;
            ValidatePcmS16Format(format);
            found_fmt = true;
        } else if (chunk_id == "data") {
            data.resize(chunk_size);
            input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
            if (!input) {
                throw std::runtime_error("unexpected end of wav data chunk");
            }
            found_data = true;
        } else {
            SkipBytes(input, chunk_size);
        }

        if (chunk_size % 2U == 1U && input.peek() != std::char_traits<char>::eof()) {
            SkipBytes(input, 1);
        }
    }

    if (!found_fmt || !found_data) {
        throw std::runtime_error("wav file missing required fmt or data chunk");
    }
    if (data.size() % format.BytesPerFrame() != 0) {
        throw std::runtime_error("wav data chunk is not aligned to frame size");
    }

    PcmAudio audio;
    audio.format = format;
    audio.bytes.assign(data.begin(), data.end());
    return audio;
}

} // namespace audio
} // namespace interview
