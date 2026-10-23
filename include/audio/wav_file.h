#pragma once

#include <string>
#include <vector>

#include "audio/audio_models.h"

namespace interview {
namespace audio {

/**
 * @brief 将 PCM 音频写为标准 RIFF/WAVE 文件
 * @param output_path 输出 wav 路径；父目录必须已存在
 * @param pcm_audio 原始 PCM 音频字节
 * @param format PCM 音频格式
 * @throws std::runtime_error 当格式不支持、路径不可写或音频字节未按帧对齐时抛出
 */
void WriteWavFile(const std::string& output_path,
                  const std::vector<unsigned char>& pcm_audio,
                  const AudioFormat& format);

/**
 * @brief 从标准 RIFF/WAVE 文件读取 16-bit PCM 音频
 * @param input_path 输入 wav 路径
 * @return 解析出的 PCM 音频和格式信息
 * @throws std::runtime_error 当文件不可读、格式不支持或数据不完整时抛出
 */
PcmAudio ReadWavFile(const std::string& input_path);

} // namespace audio
} // namespace interview
