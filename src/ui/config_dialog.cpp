#include "ui/config_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>

namespace interview {
namespace ui {

ConfigDialog::ConfigDialog(QWidget *parent)
    : QDialog(parent) {
    SetupUi();
    setWindowTitle("新建面试会话");
    resize(500, 300);
}

ConfigDialog::~ConfigDialog() {
}

void ConfigDialog::SetupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 候选人信息组
    QGroupBox* candidateGroup = new QGroupBox("候选人信息", this);
    QFormLayout* candidateLayout = new QFormLayout(candidateGroup);
    
    name_edit_ = new QLineEdit(this);
    name_edit_->setPlaceholderText("请输入候选人姓名");
    candidateLayout->addRow("姓名:", name_edit_);
    
    mainLayout->addWidget(candidateGroup);
    
    // 简历配置组
    QGroupBox* resumeGroup = new QGroupBox("简历设置", this);
    QVBoxLayout* resumeLayout = new QVBoxLayout(resumeGroup);
    
    use_resume_check_ = new QCheckBox("使用简历驱动面试", this);
    use_resume_check_->setChecked(false);
    resumeLayout->addWidget(use_resume_check_);
    
    QHBoxLayout* resumePathLayout = new QHBoxLayout();
    resume_edit_ = new QLineEdit(this);
    resume_edit_->setPlaceholderText("选择PDF简历文件");
    resume_edit_->setEnabled(false);
    resumePathLayout->addWidget(resume_edit_);
    
    browse_button_ = new QPushButton("浏览...", this);
    browse_button_->setEnabled(false);
    resumePathLayout->addWidget(browse_button_);
    resumeLayout->addLayout(resumePathLayout);
    
    QLabel* hint = new QLabel("提示: 如不提供简历，将生成默认C++技术问题", this);
    hint->setStyleSheet("QLabel { color: #666; font-style: italic; }");
    resumeLayout->addWidget(hint);
    
    mainLayout->addWidget(resumeGroup);
    
    // 问题设置组
    QGroupBox* questionGroup = new QGroupBox("问题设置", this);
    QFormLayout* questionLayout = new QFormLayout(questionGroup);
    
    questions_spin_box_ = new QSpinBox(this);
    questions_spin_box_->setRange(1, 50);
    questions_spin_box_->setValue(5);
    questions_spin_box_->setSuffix(" 个");
    questionLayout->addRow("问题数量(1~50):", questions_spin_box_);
    
    mainLayout->addWidget(questionGroup);
    
    // 按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    cancel_button_ = new QPushButton("取消", this);
    ok_button_ = new QPushButton("开始面试", this);
    ok_button_->setDefault(true);
    
    buttonLayout->addWidget(cancel_button_);
    buttonLayout->addWidget(ok_button_);
    
    mainLayout->addLayout(buttonLayout);
    
    // 连接信号
    connect(use_resume_check_, &QCheckBox::toggled, this, &ConfigDialog::OnUseResumeToggled);
    connect(browse_button_, &QPushButton::clicked, this, &ConfigDialog::OnBrowseResume);
    connect(ok_button_, &QPushButton::clicked, this, &ConfigDialog::OnAccepted);
    connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);
}

QString ConfigDialog::GetCandidateName() const {
    return name_edit_->text().trimmed();
}

QString ConfigDialog::GetResumePath() const {
    if (use_resume_check_->isChecked()) {
        return resume_edit_->text().trimmed();
    }
    return QString();
}

int ConfigDialog::GetMinQuestions() const {
    return questions_spin_box_->value();
}

bool ConfigDialog::IsUseResume() const {
    return use_resume_check_->isChecked();
}

void ConfigDialog::OnBrowseResume() {
    QString file_name = QFileDialog::getOpenFileName(
        this,
        "选择PDF简历",
        "",
        "PDF 文件 (*.pdf);;所有文件 (*)"
    );
    
    if (!file_name.isEmpty()) {
        resume_edit_->setText(file_name);
    }
}

void ConfigDialog::OnUseResumeToggled(bool checked) {
    resume_edit_->setEnabled(checked);
    browse_button_->setEnabled(checked);
    
    if (!checked) {
        resume_edit_->clear();
    }
}

void ConfigDialog::OnAccepted() {
    if (ValidateInput()) {
        accept();
    }
}

bool ConfigDialog::ValidateInput() {
    QString name = name_edit_->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入候选人姓名");
        name_edit_->setFocus();
        return false;
    }
    
    if (use_resume_check_->isChecked()) {
        QString resume = resume_edit_->text().trimmed();
        if (resume.isEmpty()) {
            QMessageBox::warning(this, "输入错误", "请选择简历文件");
            browse_button_->setFocus();
            return false;
        }
        
        QFileInfo file_info(resume);
        if (!file_info.exists()) {
            QMessageBox::warning(this, "文件错误", "简历文件不存在");
            return false;
        }
        
        if (file_info.suffix().toLower() != "pdf") {
            QMessageBox::warning(this, "文件错误", "只支持PDF格式的简历");
            return false;
        }
    }
    
    return true;
}

} // namespace ui
} // namespace interview

