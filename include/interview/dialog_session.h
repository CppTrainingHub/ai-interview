/**
 * @file dialog_session.h
 * @brief 对话会话管理模块
 *
 * 本文件定义了DialogSession类，是整个语音面试系统的核心协调器。
 * 负责整合音频采集、WebSocket通信、面试流程控制等所有子系统。
 *
 * 核心职责：
 * 1. 管理对话会话生命周期（启动、运行、停止）
 * 2. 协调音频输入输出（麦克风录音、TTS播放）
 * 3. 处理服务器事件（TTS开始/结束、ASR结果、用户说话状态）
 * 4. 控制面试流程（问题切换、回答处理、评分、追问）
 * 5. 实现状态机（空闲、说话、思考等状态转换）
 */

#pragma once

#include <memory>
#include <string>
#include <functional>
#include "common/protocol.h"

namespace interview {
namespace session {


/**
 * @brief 对话会话管理类
 *
 * 这是整个语音面试系统的中央控制器，负责：
 * - 建立WebSocket连接和对话会话
 * - 管理音频流（麦克风输入和扬声器输出）
 * - 处理服务器事件并触发相应动作
 * - 控制面试流程（问题、回答、评分、追问）
 * - 维护会话状态机，确保正确的交互时序
 *
 * 工作流程：
 * 1. 构造时初始化面试控制器
 * 2. Start()建立连接、启动音频线程、发送开场白
 * 3. 运行时通过状态机控制面试流程
 * 4. 接收服务器事件，触发状态转换和动作
 * 5. Stop()关闭连接、保存报告
 *
 * 使用示例：
 * @code
 * // 创建面试会话
 * session::DialogSession session("张三");
 *
 * // 配置简历驱动面试
 * session.ConfigureResumeInterview("resume.pdf", 50);
 *
 * // 启动会话
 * session.Start();
 *
 * // 等待会话结束
 * while (session.IsRunning()) {
 *     std::this_thread::sleep_for(std::chrono::milliseconds(100));
 * }
 * @endcode
 */
class DialogSession {
public:
    /**
     * @brief 对话内容回调类型（带题号）
     * @param role 角色标识："interviewer" 或 "candidate"
     * @param text 说话内容文本
     * @param question_index 当前题目序号
     */
    using DialogContentCallback = std::function<void(const std::string& role, const std::string& text, int question_index)>;

    /**
     * @brief 构造函数
     *
     * 创建对话会话实例，初始化内部状态，创建面试会话和流程控制器。
     *
     * @param candidate_name 候选人姓名，用于面试报告和个性化交互
     */
    DialogSession(const std::string& candidate_name = "候选人");

    /**
     * @brief 析构函数
     *
     * 自动停止会话、清理资源。
     * 如果会话仍在运行，会先调用Stop()。
     */
    ~DialogSession();

    /**
     * @brief 配置简历驱动面试
     *
     * 从PDF简历文件中提取内容，使用LLM生成针对性的面试问题。
     * 生成的问题会涵盖简历中提到的技术栈、项目经验等。
     *
     * 工作流程：
     * 1. 使用PDFParser解析PDF文件，提取文本内容
     * 2. 调用LLM API，根据简历内容生成min_questions个问题
     * 3. 问题包含难度分级、类别、考察点等元数据
     * 4. 存储问题列表，供面试流程使用
     *
     * @param resume_pdf_path PDF简历文件的完整路径
     * @param min_questions 最少生成的问题数量（默认10个，会根据简历内容调整）
     * @throws std::runtime_error 如果PDF解析失败或LLM生成失败
     */
    void ConfigureResumeInterview(const std::string& resume_pdf_path, int min_questions = 10);

    /**
     * @brief 配置默认面试
     *
     * 生成默认的C++面试题。
     *
     * @param num_questions 最少生成的问题数量（默认10个）
     * @throws std::runtime_error 如果LLM生成失败
     */
    void ConfigureDefaultInterview(int min_questions = 10);

    /**
     * @brief 启动会话
     *
     * 建立WebSocket连接，启动音频线程，发送面试开场白。
     *
     * @throws std::runtime_error 
     */
    void Start();

    /**
     * @brief 处理服务器响应
     *
     * 由WebSocket客户端的接收线程调用，处理服务器推送的事件和数据。
     * 这是事件驱动架构的核心方法，根据不同事件触发不同动作。
     * @param response 服务器响应的解析结果
     */
    void HandleServerResponse(const common::ParsedResponse& response);

    /**
     * @brief 设置对话内容回调
     * 
     * 设置回调函数，用于实时接收面试官和候选人的对话内容。
     * @param callback 回调函数，参数为(role, text, question_index)
     */
    void SetDialogContentCallback(DialogContentCallback callback);

    /**
     * @brief 停止会话
     * 停止音频线程，关闭WebSocket连接，保存面试报告。
     */
    void Stop();

    /**
     * @brief 检查会话是否正在运行
     *
     * 查询会话的运行状态。主线程可以通过轮询此方法等待会话结束。
     *
     * @return true=会话正在运行，false=会话已停止
     */
    bool IsRunning() const;

private:
    class DialogSessionImpl;
    std::unique_ptr<DialogSessionImpl> pimpl_;
};

} // namespace session
} // namespace interview
