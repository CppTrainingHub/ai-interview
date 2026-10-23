#include "gui/config_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace interview {
namespace gui {

class ConfigDialog::Impl {
public:
    explicit Impl(ConfigDialog& owner) : dialog(owner) {
        auto* root = new QVBoxLayout(&dialog);

        auto* candidate_group = new QGroupBox(tr("候选人信息"), &dialog);
        auto* candidate_form = new QFormLayout(candidate_group);
        name = new QLineEdit(candidate_group);
        name->setPlaceholderText(tr("请输入候选人姓名"));
        candidate_form->addRow(tr("姓名："), name);
        root->addWidget(candidate_group);

        auto* resume_group = new QGroupBox(tr("简历设置"), &dialog);
        auto* resume_layout = new QVBoxLayout(resume_group);
        use_resume = new QCheckBox(tr("使用 PDF 简历驱动面试"), resume_group);
        resume_layout->addWidget(use_resume);
        auto* path_layout = new QHBoxLayout();
        resume_path = new QLineEdit(resume_group);
        resume_path->setEnabled(false);
        browse = new QPushButton(tr("浏览…"), resume_group);
        browse->setEnabled(false);
        path_layout->addWidget(resume_path, 1);
        path_layout->addWidget(browse);
        resume_layout->addLayout(path_layout);
        root->addWidget(resume_group);

        auto* options_group = new QGroupBox(tr("面试设置"), &dialog);
        auto* options_form = new QFormLayout(options_group);
        questions = new QSpinBox(options_group);
        questions->setRange(1, 50);
        questions->setValue(5);
        questions->setSuffix(tr(" 题"));
        audio_mode = new QCheckBox(tr("启用麦克风和语音播放"), options_group);
        audio_mode->setChecked(true);
        options_form->addRow(tr("最少题目数："), questions);
        options_form->addRow(tr("交互模式："), audio_mode);
        root->addWidget(options_group);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        buttons->button(QDialogButtonBox::Ok)->setText(tr("开始面试"));
        root->addWidget(buttons);

        QObject::connect(use_resume, &QCheckBox::toggled, resume_path, &QWidget::setEnabled);
        QObject::connect(use_resume, &QCheckBox::toggled, browse, &QWidget::setEnabled);
        QObject::connect(browse, &QPushButton::clicked, [&owner, this]() {
            const QString path = QFileDialog::getOpenFileName(
                &owner, tr("选择 PDF 简历"), QString(), tr("PDF 文件 (*.pdf)"));
            if (!path.isEmpty()) {
                resume_path->setText(path);
            }
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        QObject::connect(buttons, &QDialogButtonBox::accepted, [&owner, this]() {
            if (Validate()) {
                owner.accept();
            }
        });

        dialog.setWindowTitle(tr("新建面试会话"));
        dialog.setMinimumWidth(520);
    }

    GuiInterviewConfig Value() const {
        return {name->text().trimmed(),
                use_resume->isChecked() ? resume_path->text().trimmed() : QString(),
                questions->value(),
                audio_mode->isChecked()};
    }

private:
    static QString tr(const char* text) { return ConfigDialog::tr(text); }

    bool Validate() {
        if (name->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, tr("输入错误"), tr("请输入候选人姓名。"));
            name->setFocus();
            return false;
        }
        if (use_resume->isChecked()) {
            const QFileInfo file(resume_path->text().trimmed());
            if (!file.exists() || !file.isFile()) {
                QMessageBox::warning(&dialog, tr("文件错误"), tr("请选择存在的 PDF 简历。"));
                return false;
            }
            if (file.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) != 0) {
                QMessageBox::warning(&dialog, tr("文件错误"), tr("简历必须是 PDF 文件。"));
                return false;
            }
        }
        return true;
    }

    ConfigDialog& dialog;
    QLineEdit* name = nullptr;
    QCheckBox* use_resume = nullptr;
    QLineEdit* resume_path = nullptr;
    QPushButton* browse = nullptr;
    QSpinBox* questions = nullptr;
    QCheckBox* audio_mode = nullptr;
};

ConfigDialog::ConfigDialog(QWidget* parent)
    : QDialog(parent), pImpl(std::make_unique<Impl>(*this)) {}
ConfigDialog::~ConfigDialog() = default;
GuiInterviewConfig ConfigDialog::InterviewConfig() const { return pImpl->Value(); }

} // namespace gui
} // namespace interview
