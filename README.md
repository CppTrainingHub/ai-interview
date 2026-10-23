# AI Interview：C++ AI 面试训练系统

基于 **C++17** 的技术面试训练项目，支持根据 PDF 简历生成问题、文本或语音回答、回答评分、补充追问以及 JSON 报告导出。CLI、HTTP 服务和可选的 Qt 桌面界面复用同一套面试业务流程。

项目优先保证 CLI 与后端流程可运行；GUI、麦克风和云端语音能力通过可选构建开关启用。离线模式可用于学习代码、演示和验证流程，无需申请大模型 API 密钥。

## 功能与运行模式

| 功能 | 当前实现 |
| --- | --- |
| 面试流程 | 开场、自我介绍、技术问题、评分、追问、总结 |
| 简历驱动 | 使用 PoDoFo 提取文本型 PDF 的文字，交给问题生成器 |
| 大模型接入 | 使用 libcurl 调用兼容 Chat Completions 格式的服务 |
| 离线演示 | 固定题库、规则评分和离线总结，不调用外部模型 |
| 交互入口 | 命令行、HTTP JSON 接口、可选 Qt6 Widgets GUI |
| 语音管线 | 空设备、WAV 文件、可选 PortAudio 麦克风设备；脚本 ASR、静音 PCM TTS，以及可选火山引擎实时语音接入 |
| 报告 | 候选人信息、问答记录、逐题评分、总分、平均分与总结 |
| 工程支持 | CMake presets、vcpkg、CTest、跨平台 CI 和 GUI 打包配置 |

离线模式的评分用于验证程序流程，不代表实际能力评价。离线模式中的简历解析也使用占位实现，不会真正读取 PDF；真实 PDF 解析需使用 `openai` 依赖模式并安装 PoDoFo。

## 目录结构与模块职责

```text
AI-Interview/
├── .github/workflows/       # 跨平台构建、测试和打包
├── config/                 # 可共享的配置模板；本地密钥配置不提交
├── include/                # 各模块的公共接口和领域模型
├── src/
│   ├── app/                # CLI、HTTP、GUI 入口及 CLI 展示逻辑
│   ├── config/             # 配置文件与环境变量读取
│   ├── core/               # 会话状态、流程编排、业务端口、报告数据构建
│   ├── runtime/            # 依赖装配，以及各入口共用的运行时
│   ├── llm/                # 模型调用、出题、评分、总结和响应解析
│   ├── pdf/                # PDF 简历文本提取
│   ├── audio/              # 录放音、ASR/TTS、WAV 与语音协议
│   ├── net/                # HTTP 服务与面试会话控制
│   ├── gui/                # Qt 界面和后台工作线程
│   └── report/             # JSON 报告写入
├── tests/                  # 单元测试和模块验证程序
├── packaging/              # macOS、Linux 等平台打包资源
├── CMakeLists.txt          # 顶层构建配置
├── CMakePresets.json       # 可共享的构建预设
└── vcpkg.json              # 依赖清单
```

核心流程通过 `ResumeParser`、`QuestionGenerator`、`AnswerEvaluator`、`SummaryGenerator` 等端口与具体实现解耦。`InterviewSession` 保存状态，`InterviewOrchestrator` 推进面试，`InterviewRuntime` 装配真实或离线依赖；入口层负责参数、展示或网络协议。非平凡类按项目约定采用 PImpl 隐藏实现细节。

## 环境与依赖

- 支持 C++17 的编译器，例如 GCC、Clang 或 MSVC。
- 建议 CMake **3.25 或更高版本**：顶层工程最低声明为 3.20，但仓库 presets 使用版本 6。
- Git，以及已安装、完成 bootstrap 的 vcpkg。
- Unix Makefiles 预设需要 Make；Windows 预设使用 Visual Studio 2022。

依赖由 `vcpkg.json` 声明：`curl`、`nlohmann-json`、`portaudio`、`qtbase` 和 `podofo`。清单包含完整依赖，因此即使关闭 GUI 或真实录音，vcpkg 仍可能安装 Qt、PortAudio 等包；首次配置耗时取决于依赖缓存与机器环境。

PoDoFo 在 CMake 中按可选依赖查找：未找到时仍可构建不依赖 PDF 的流程，但真实简历提取会报错。扫描图片型 PDF 当前没有 OCR 支持。

## 快速开始：离线 CLI

以下命令使用 macOS/Linux shell，并从项目根目录执行。Windows 可使用 PowerShell 设置环境变量，随后使用 Windows 预设。

### 1. 获取源码并设置 vcpkg 路径

```bash
git clone git@github.com:CppTrainingHub/ai-interview.git
cd ai-interview

# 替换为本机已安装的 vcpkg 目录
export VCPKG_ROOT="$HOME/vcpkg"
```


### 2. 配置、编译与测试

Apple Silicon macOS：

```bash
cmake --preset macos-arm64-debug
cmake --build --preset macos-arm64-debug --parallel
ctest --preset macos-arm64-debug
```

其他平台选择对应预设，配置、编译和测试使用同一个预设名：

| 平台 | 预设名 | vcpkg triplet |
| --- | --- | --- |
| Linux ARM64 | `linux-debug` | `arm64-linux` |
| Linux x64 | `linux-x64-debug` | `x64-linux` |
| macOS ARM64 | `macos-arm64-debug` | `arm64-osx` |
| Windows x64 | `windows-x64-debug` | `x64-windows` |

例如 Linux x64：

```bash
cmake --preset linux-x64-debug
cmake --build --preset linux-x64-debug --parallel
ctest --preset linux-x64-debug
```

Windows PowerShell：

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug --config Debug --parallel
ctest --preset windows-x64-debug -C Debug
```

### 3. 启动无需密钥的面试

macOS ARM64：

```bash
./build/macos-arm64-debug/src/app/ai_interview_cli \
  --config config/app.scripted.example.env \
  --name Demo \
  --questions 3
```

Linux 将路径中的 `macos-arm64-debug` 替换为对应预设；Windows Debug 可执行文件位于 `build/windows-x64-debug/src/app/Debug/ai_interview_cli.exe`。

首先输入一段自我介绍，再按提示回答技术题和追问。第一段自我介绍用于推进流程，不参与评分。结束时程序打印总结，并生成：

```text
reports/Demo_cli_interview_report.json
```

`--questions` 控制请求生成的主问题数量，追问会额外增加回答记录。

## 连接真实大模型与 PDF 简历

复制模板到被 Git 忽略的本地配置文件：

```bash
cp config/app.example.env config/app.local.env
```

编辑 `config/app.local.env`，至少设置：

```dotenv
AI_INTERVIEW_RUNTIME_MODE=text
AI_INTERVIEW_DEPENDENCY_MODE=openai
AI_INTERVIEW_LLM_API_URL=https://your-provider.example/v1/chat/completions
AI_INTERVIEW_LLM_API_KEY=替换为实际密钥
AI_INTERVIEW_LLM_MODEL=替换为服务支持的模型名
AI_INTERVIEW_LLM_TEMPERATURE=0.2
AI_INTERVIEW_LLM_MAX_TOKENS=2048
AI_INTERVIEW_LLM_TIMEOUT_SECONDS=60
AI_INTERVIEW_REPORT_DIR=reports
```

接口 URL 必须是完整请求地址；程序直接向该地址 POST，不会自动补上 `/chat/completions`。服务需要接受 `messages`、`model`、`temperature`、`max_tokens` 字段，并返回 `choices[0].message.content`。

不传简历时生成默认技术问题；传入简历时，根据提取的文本生成问题：

```bash
./build/macos-arm64-debug/src/app/ai_interview_cli \
  --config config/app.local.env \
  --resume /absolute/path/to/resume.pdf \
  --name Candidate \
  --questions 5
```

简历需可读取、有 `.pdf` 扩展名且包含可提取的文字。简历文本和回答会按流程发送给所配置的模型服务，请使用适合共享给该服务的数据。

### 配置加载规则

程序按以下顺序选择一个配置来源：

1. 显式 `--config` 指定的文件。
2. 当前工作目录下的 `config/app.local.env`。
3. 环境变量。

配置文件与环境变量**不会合并覆盖**。如果本地配置文件存在，单独修改环境变量不会覆盖该文件。GUI 入口使用默认配置加载规则，不解析 CLI 的 `--config` 参数。

文件格式为每行 `KEY=value`，空行和以 `#` 开头的注释会被跳过。值不需要添加引号；解析器不执行 shell 变量展开。

查看入口参数：

```bash
./build/macos-arm64-debug/src/app/ai_interview_cli --help
./build/macos-arm64-debug/src/app/ai_interview_http_server --help
```

## HTTP 服务

使用离线配置启动，便于先验证接口：

```bash
./build/macos-arm64-debug/src/app/ai_interview_http_server \
  --config config/app.scripted.example.env \
  --host 127.0.0.1 \
  --port 8080
```

| 方法 | 路径 | 用途 |
| --- | --- | --- |
| GET | `/health` | 检查服务状态 |
| POST | `/interview/start` | 创建面试会话 |
| POST | `/interview/answer` | 提交自我介绍、技术题回答或追问回答 |
| POST | `/interview/report` | 将当前会话报告写入文件 |

在另一个终端调用：

```bash
curl http://127.0.0.1:8080/health

curl -X POST http://127.0.0.1:8080/interview/start \
  -H 'Content-Type: application/json' \
  -d '{"candidate_name":"Demo","resume_path":"","min_questions":3}'
```

从返回值中取得 `session_id`，替换下面的 `session-1`：

```bash
curl -X POST http://127.0.0.1:8080/interview/answer \
  -H 'Content-Type: application/json' \
  -d '{"session_id":"session-1","answer":"我有 C++ 开发经验，希望练习资源管理与并发。"}'
```

随后继续调用 `/interview/answer` 回答返回的问题，直到响应中的 `finished` 为 `true`，再导出报告：

```bash
curl -X POST http://127.0.0.1:8080/interview/report \
  -H 'Content-Type: application/json' \
  -d '{"session_id":"session-1"}'
```

响应包含动作类型、展示文本、会话 ID 等字段；报告接口返回服务端文件路径。`resume_path` 同样是服务端可访问的文件路径，当前不提供简历文件上传接口。会话保存在进程内存中，重启服务后不能继续旧会话。当前服务适合本地开发和集成验证，尚未提供用户认证、TLS 和持久化会话。

## 可选 Qt GUI

GUI 默认关闭，使用以下命令启用：

```bash
cmake --preset macos-arm64-debug -DAI_INTERVIEW_BUILD_GUI=ON
cmake --build --preset macos-arm64-debug --parallel
ctest --preset macos-arm64-debug
```

先准备默认配置文件；首次体验可复制离线模板。如果已有本地配置，直接编辑现有文件即可：

```bash
cp config/app.scripted.example.env config/app.local.env
./build/macos-arm64-debug/src/gui/ai_interview_gui.app/Contents/MacOS/ai_interview_gui
```

从项目根目录启动可使默认配置路径正确解析。Linux 的 GUI 程序通常为 `build/<预设名>/src/gui/ai_interview_gui`；Windows Debug 程序通常为 `build/windows-x64-debug/src/gui/Debug/ai_interview_gui.exe`。

GUI 提供面试展示、回答交互和配置界面；真实麦克风及云端语音还需启用下述构建开关并填写配置。

## 可选语音能力

### 构建开关

| CMake 开关 | 默认值 | 作用 |
| --- | --- | --- |
| `AI_INTERVIEW_BUILD_TESTS` | `ON` | 构建测试程序 |
| `AI_INTERVIEW_BUILD_GUI` | `OFF` | 构建 Qt GUI |
| `AI_INTERVIEW_ENABLE_PORTAUDIO` | `OFF` | 编译真实录放音设备实现 |
| `AI_INTERVIEW_ENABLE_VOLCENGINE_REALTIME` | `OFF` | 编译火山引擎实时语音传输实现 |

例如：

```bash
cmake --preset macos-arm64-debug \
  -DAI_INTERVIEW_BUILD_GUI=ON \
  -DAI_INTERVIEW_ENABLE_PORTAUDIO=ON \
  -DAI_INTERVIEW_ENABLE_VOLCENGINE_REALTIME=ON
cmake --build --preset macos-arm64-debug --parallel
```

实时语音需要支持 WebSocket 的 libcurl 构建以及有效的服务凭据；仅开启开关不能保证账号权限、设备与云端服务可用。

### 运行时配置

以下配置添加到当前使用的本地配置文件，LLM 配置仍按所选依赖模式保留：

```dotenv
AI_INTERVIEW_RUNTIME_MODE=audio
AI_INTERVIEW_AUDIO_DEVICE=portaudio
AI_INTERVIEW_ASR_PROVIDER=volcengine
AI_INTERVIEW_TTS_PROVIDER=volcengine
AI_INTERVIEW_VOLCENGINE_WEBSOCKET_URL=wss://openspeech.bytedance.com/api/v3/realtime/dialogue
AI_INTERVIEW_VOLCENGINE_APP_ID=替换为应用ID
AI_INTERVIEW_VOLCENGINE_ACCESS_KEY=替换为访问密钥
AI_INTERVIEW_VOLCENGINE_APP_KEY=替换为应用密钥
AI_INTERVIEW_VOLCENGINE_RESOURCE_ID=volc.speech.dialog
AI_INTERVIEW_VOLCENGINE_SPEAKER=zh_male_yunzhou_jupiter_bigtts
AI_INTERVIEW_VOLCENGINE_OPERATION_TIMEOUT_MS=30000
AI_INTERVIEW_AUDIO_INPUT_SAMPLE_RATE_HZ=16000
AI_INTERVIEW_AUDIO_OUTPUT_SAMPLE_RATE_HZ=24000
```

注意：当前 `config/app.example.env` 末尾的 `AI_INTERVIEW_SPEECH_*` 名称尚未与配置读取代码同步，实际应使用上面的 **`AI_INTERVIEW_VOLCENGINE_*`** 名称。

不连接真实语音服务时，可使用 `AI_INTERVIEW_AUDIO_DEVICE=null`、`AI_INTERVIEW_ASR_PROVIDER=scripted` 和 `AI_INTERVIEW_TTS_PROVIDER=silent_pcm` 验证语音流程。脚本识别结果通过 `AI_INTERVIEW_SCRIPTED_TRANSCRIPTS` 配置，多个回答用 `||` 分隔；这不是对真实音频的识别，静音 PCM 也不会生成可听语音。

使用 WAV 设备时设置 `AI_INTERVIEW_AUDIO_DEVICE=wav`，并填写 `AI_INTERVIEW_INPUT_WAV_PATH`、`AI_INTERVIEW_OUTPUT_WAV_PATH`。真实麦克风还需操作系统授权和可用的输入输出设备。

## JSON 报告

报告默认保存在 `reports/`，可通过 `AI_INTERVIEW_REPORT_DIR` 修改。主要字段如下：

| 字段 | 含义 |
| --- | --- |
| `candidate_name`、`interview_date` | 候选人姓名和报告时间 |
| `duration_minutes` | 面试时长 |
| `total_question` | 已记录的评分回答数量，包含追问回答 |
| `total_score`、`average_score` | 所有记录的总分、平均分 |
| `evaluation` | 根据平均分生成的综合评价 |
| `summary_text` | 模型或离线总结器生成的总结 |
| `records` | 问题、回答、时间、评分、反馈和追问标记 |

CLI 完成面试后自动导出报告；HTTP 通过报告接口导出。相同候选人重复运行 CLI 时默认文件名相同，可能覆盖上次报告，需自行归档。

## 测试与打包

测试覆盖会话与流程编排、报告、LLM 响应解析、PDF 输入校验、音频模块、运行时及网络控制；启用 GUI 时还会构建 GUI 测试。普通测试不要求真实模型凭据；真实 PDF 提取可通过环境变量指定本机样例：

```bash
AI_INTERVIEW_TEST_RESUME_PDF=/absolute/path/to/resume.pdf \
  ctest --preset macos-arm64-debug
```

仓库部分测试使用固定的 `/tmp/` 路径，Windows 上可能需要适配临时目录；存在跨平台 CI 配置不等于所有平台在任意环境下都已验证通过。

启用 GUI 并完成构建后，可在目标平台执行 CPack，例如：

```bash
cd build/macos-arm64-debug
cpack --config CPackConfig.cmake -C Debug
```

打包配置包含 macOS DMG、Linux TGZ/DEB、Windows ZIP/NSIS；Windows 安装程序需要相应 NSIS 工具。安装包应在对应操作系统生成，Qt 运行库部署和其他动态库依赖也需在目标机器上验证。CI 工作流位于 `.github/workflows/gui-cross-platform.yml`。

## 常见问题

- **找不到 vcpkg 工具链**：检查 `VCPKG_ROOT` 是否指向已 bootstrap 的 vcpkg 目录。切换工具链或 triplet 时使用独立构建目录。
- **CMake 不支持 preset 版本**：升级到支持版本 6 的 CMake，建议 3.25 或以上。
- **提示缺少 API 配置**：先使用离线模板运行；真实模型模式需填写 API URL、密钥和模型名称。
- **修改环境变量未生效**：检查默认本地配置文件是否存在；配置文件优先，且不与环境变量合并。
- **PDF 无法提取文字**：确认 PoDoFo 已被 CMake 找到、路径有效，且简历不是只有扫描图片的 PDF。
- **音频设备或实时语音不可用**：检查构建开关、运行时 provider、`AI_INTERVIEW_VOLCENGINE_*` 凭据、libcurl WebSocket 支持和麦克风授权。
- **HTTP 报告路径在本机不存在**：返回的是服务端路径；远程调用不会自动下载报告文件。


