#pragma once

#include <memory>
#include <string>

#include "config/app_config.h"

namespace interview {
namespace app {

/**
 * @brief CLI 应用入口类
 * @note 负责命令行交互和模块组装，不承载 core 业务逻辑。
 */
class CliApp {
public:
    explicit CliApp(config::AppConfig config);

    ~CliApp();

    CliApp(const CliApp&) = delete;
    CliApp& operator=(const CliApp&) = delete;

    CliApp(CliApp&&) noexcept;
    CliApp& operator=(CliApp&&) noexcept;

    /**
     * @brief 运行 CLI 面试流程
     * @param resume_path 简历路径：为空时使用默认面试
     * @param candidate_name 候选人姓名
     * @param min_questions 最少题目数量
     * @return 进城退出码
     */
    int Run(const std::string& resume_path,
            const std::string& candidate_name,
            int min_questions);
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace app
} // namespace interview