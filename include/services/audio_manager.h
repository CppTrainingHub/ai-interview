/**
 * @file audio_manager.h
 * @brief 跨平台音频设备管理模块
 *
 * 音频设备管理类，封装PortAudio库。
 * 负责麦克风录音和扬声器播放的底层操作。
 *
 * 核心功能：
 * 1. 音频设备初始化和清理
 * 2. 麦克风音频采集（16kHz, int16 PCM）
 * 3. 扬声器音频播放（24kHz, float32 PCM）
 * 4. 音频格式转换和缓冲管理
 */

#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include "common/config.h"

namespace interview {
namespace services {

using AudioConfig = common::AudioConfig;

/**
 * @brief 音频设备管理类
 *
 * 封装PortAudio库，提供简单的音频I/O接口。
 *
 * 功能特性：
 * 1. 自动初始化/终止PortAudio
 * 2. 自动选择默认输入/输出设备
 * 3. 阻塞式读写（简化线程模型）
 * 4. 支持不同输入输出配置
 *
 * 音频格式：
 * - 输入：paInt16（16位有符号整数，-32768到32767）
 * - 输出：paFloat32（32位浮点数，-1.0到1.0）
 *
 * 线程模型：
 * - 麦克风线程：循环调用ReadAudio()
 * - 播放线程：循环调用WriteAudio()
 * - 每次读写都是阻塞的，直到缓冲区满/空
 *
 * 错误处理：
 * - 初始化失败抛出异常
 * - 读写overflow/underflow记录警告
 * - 其他错误抛出std::runtime_error
 *
 * 使用示例：
 * @code
 * // 创建音频管理器
 * AudioConfig input_cfg{3200, 1, 16000, 16, "pcm"};
 * AudioConfig output_cfg{4800, 1, 24000, 16, "pcm"};
 * AudioDeviceManager manager(input_cfg, output_cfg);
 *
 * // 打开音频流
 * manager.OpenInputStream();
 * manager.OpenOutputStream();
 *
 * // 麦克风线程
 * std::thread mic_thread([&]() {
 *     while (running) {
 *         auto audio = manager.ReadAudio();  // 阻塞读取3200个采样点
 *         ProcessAudio(audio);
 *     }
 * });
 *
 * // 播放线程
 * std::thread play_thread([&]() {
 *     while (running) {
 *         std::vector<float> audio = GetNextAudio();
 *         manager.WriteAudio(audio);  // 阻塞写入
 *     }
 * });
 *
 * // 清理
 * manager.Cleanup();
 * @endcode
 */
class AudioDeviceManager {
public:
    /**
     * @brief 构造函数
     *
     * 创建音频设备管理器，初始化PortAudio库。
     *
     * 初始化流程：
     * 1. 调用Pa_Initialize()
     * 2. 保存输入输出配置
     * 3. 记录初始化成功日志
     *
     * @param input_config 输入音频配置（麦克风）
     * @param output_config 输出音频配置（扬声器）
     * @throws std::runtime_error PortAudio初始化失败
     */
    AudioDeviceManager(const AudioConfig& input_config,
                       const AudioConfig& output_config);

    /**
     * @brief 析构函数
     *
     * 自动调用Cleanup()清理资源。
     */
    ~AudioDeviceManager();

    /**
     * @brief 打开输入流（麦克风）
     *
     * 打开默认输入设备并启动音频流。
     *
     * 配置流程：
     * 1. 获取默认输入设备
     * 2. 配置流参数（采样率、声道、格式）
     * 3. 打开音频流（Pa_OpenStream）
     * 4. 启动音频流（Pa_StartStream）
     *
     * 流参数：
     * - 设备：系统默认输入设备（麦克风）
     * - 声道数：1（单声道）
     * - 格式：paInt16（16位整数）
     * - 采样率：16000 Hz
     * - 缓冲区：3200帧（200ms @ 16kHz）
     * - 延迟：低延迟模式
     *
     * @throws std::runtime_error 没有输入设备或打开失败
     */
    void OpenInputStream();

    /**
     * @brief 打开输出流（扬声器）
     *
     * 打开默认输出设备并启动音频流。
     *
     * 配置流程：
     * 1. 获取默认输出设备
     * 2. 配置流参数（采样率、声道、格式）
     * 3. 打开音频流（Pa_OpenStream）
     * 4. 启动音频流（Pa_StartStream）
     *
     * 流参数：
     * - 设备：系统默认输出设备（扬声器/耳机）
     * - 声道数：1（单声道）
     * - 格式：paFloat32（32位浮点）
     * - 采样率：24000 Hz（匹配TTS输出）
     * - 缓冲区：4800帧（200ms @ 24kHz）
     * - 延迟：低延迟模式
     *
     * @throws std::runtime_error 没有输出设备或打开失败
     */
    void OpenOutputStream();

    /**
     * @brief 读取音频数据（阻塞）
     *
     * 从麦克风读取一个缓冲区的音频数据。
     * 此方法会阻塞，直到读取到足够的数据。
     *
     * 读取流程：
     * 1. 调用Pa_ReadStream()
     * 2. 等待缓冲区填满（约200ms @ 16kHz）
     * 3. 返回int16数组
     *
     * 返回数据：
     * - 格式：int16_t数组
     * - 大小：chunk * channels（默认3200个采样点）
     * - 时长：200ms（3200 / 16000 = 0.2秒）
     * - 字节数：6400字节（3200 * 2）
     *
     * 错误处理：
     * - Input overflow（缓冲区溢出）：记录警告但继续返回数据
     * - 其他错误：抛出异常
     *
     * 使用场景：
     * - 麦克风线程循环调用
     * - 每10ms发送一次给服务器
     *
     * @return int16音频样本数组（3200个采样点）
     * @throws std::runtime_error 输入流未打开或读取失败
     */
    std::vector<int16_t> ReadAudio();

    /**
     * @brief 写入音频数据（阻塞）
     *
     * 将音频数据写入扬声器播放。
     * 此方法会阻塞，直到数据完全写入缓冲区。
     *
     * 写入流程：
     * 1. 调用Pa_WriteStream()
     * 2. 等待缓冲区有空间
     * 3. 写入float32数组
     *
     * 输入数据：
     * - 格式：float数组（-1.0到1.0）
     * - 声道：单声道
     * - 采样率：24000 Hz（必须与OpenOutputStream配置一致）
     *
     * 错误处理：
     * - Output underflow（缓冲区下溢）：记录警告
     * - 其他错误：抛出异常
     *
     * 使用场景：
     * - 播放线程循环调用
     * - 播放TTS返回的音频数据
     *
     * @param audio float32音频样本数组（-1.0到1.0范围）
     * @throws std::runtime_error 输出流未打开或写入失败
     */
    void WriteAudio(const std::vector<float>& audio);

    /**
     * @brief 清理资源
     *
     * 关闭音频流，终止PortAudio库。
     *
     * 清理流程：
     * 1. 停止输入流（Pa_StopStream）
     * 2. 关闭输入流（Pa_CloseStream）
     * 3. 停止输出流
     * 4. 关闭输出流
     * 5. 终止PortAudio（Pa_Terminate）
     *
     * 调用时机：
     * - 会话结束时
     * - 析构函数自动调用
     * - 可重复调用（内部有状态检查）
     */
    void Cleanup();

private:
    class AudioManagerImpl;
    std::unique_ptr<AudioManagerImpl> pimpl_;
};

} // namespace services
} // namespace interview
