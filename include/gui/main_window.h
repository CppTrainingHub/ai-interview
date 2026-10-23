#pragma once

#include <memory>

#include <QMainWindow>

#include "config/app_config.h"
#include "gui/gui_models.h"

namespace interview {
namespace gui {

/**
 * @brief 跨平台 AI 面试桌面主窗口。
 * @note 窗口只编排交互和展示，业务处理由工作线程中的 Runtime 完成。
 */
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief 创建桌面主窗口并启动后台工作线程。
     * @param config 从环境变量加载的应用配置。
     * @param parent 可选 Qt 父窗口。
     */
    explicit MainWindow(config::AppConfig config, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace gui
} // namespace interview
