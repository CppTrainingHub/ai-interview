#pragma once

#include <memory>

#include <QDialog>

#include "gui/gui_models.h"

namespace interview {
namespace gui {

/**
 * @brief 收集候选人、简历、题量和交互模式的配置对话框。
 * @note 对话框只负责输入与校验，不创建 InterviewRuntime。
 */
class ConfigDialog final : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief 创建会话配置对话框。
     * @param parent 所属窗口，可为空。
     */
    explicit ConfigDialog(QWidget* parent = nullptr);
    ~ConfigDialog() override;

    /**
     * @brief 返回经过校验的 GUI 会话配置。
     * @return 当前控件内容组成的值对象。
     */
    GuiInterviewConfig InterviewConfig() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace gui
} // namespace interview
