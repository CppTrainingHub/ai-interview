#include "gui/main_window.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QTextBrowser>
#include <QTextEdit>
#include <QThread>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#include "gui/config_dialog.h"
#include "gui/interview_worker.h"

namespace interview {
namespace gui {

class MainWindow::Impl {
public:
    Impl(MainWindow& owner_value, config::AppConfig config)
        : owner(owner_value), worker(new InterviewWorker(std::move(config))) {
        BuildUi();
        ConnectWorker();
        worker->moveToThread(&worker_thread);
        QObject::connect(&worker_thread, &QThread::finished, worker, &QObject::deleteLater);
        worker_thread.start();
        UpdateState(GuiInterviewState::kReady);
    }

    ~Impl() {
        if (worker_thread.isRunning()) {
            QMetaObject::invokeMethod(
                worker, &InterviewWorker::StopInterview, Qt::BlockingQueuedConnection);
            worker_thread.quit();
            worker_thread.wait();
        }
    }

private:
    static QString StateText(GuiInterviewState state) {
        switch (state) {
            case GuiInterviewState::kReady: return MainWindow::tr("就绪");
            case GuiInterviewState::kPreparing: return MainWindow::tr("正在准备面试");
            case GuiInterviewState::kInterviewerSpeaking: return MainWindow::tr("面试官正在朗读");
            case GuiInterviewState::kWaitingForAnswer: return MainWindow::tr("等待候选人回答");
            case GuiInterviewState::kRecording: return MainWindow::tr("正在录音");
            case GuiInterviewState::kStoppingRecording: return MainWindow::tr("正在结束录音");
            case GuiInterviewState::kRecognizing: return MainWindow::tr("正在识别语音");
            case GuiInterviewState::kEvaluating: return MainWindow::tr("正在评估回答");
            case GuiInterviewState::kCompleted: return MainWindow::tr("面试已完成");
            case GuiInterviewState::kError: return MainWindow::tr("发生错误");
        }
        return {};
    }

    void BuildUi() {
        owner.setWindowTitle(MainWindow::tr("AI C++ 面试训练系统"));
        owner.resize(1050, 720);

        auto* file_menu = owner.menuBar()->addMenu(MainWindow::tr("文件(&F)"));
        auto* new_action = file_menu->addAction(MainWindow::tr("新建面试(&N)"));
        new_action->setShortcut(QKeySequence::New);
        auto* stop_action = file_menu->addAction(MainWindow::tr("停止当前面试"));
        file_menu->addSeparator();
        file_menu->addAction(MainWindow::tr("退出(&X)"), &owner, &QWidget::close);

        auto* help_menu = owner.menuBar()->addMenu(MainWindow::tr("帮助(&H)"));
        help_menu->addAction(MainWindow::tr("关于"), [&]() {
            QMessageBox::about(&owner, MainWindow::tr("关于"),
                MainWindow::tr("跨平台 C++17 AI 面试训练系统\n支持文本与语音面试。"));
        });

        auto* toolbar = owner.addToolBar(MainWindow::tr("面试控制"));
        toolbar->setMovable(false);
        new_button = new QPushButton(MainWindow::tr("新建面试"), &owner);
        start_record_button = new QPushButton(MainWindow::tr("开始回答"), &owner);
        stop_record_button = new QPushButton(MainWindow::tr("结束回答"), &owner);
        toolbar->addWidget(new_button);
        toolbar->addSeparator();
        toolbar->addWidget(start_record_button);
        toolbar->addWidget(stop_record_button);

        auto* central = new QWidget(&owner);
        auto* root = new QVBoxLayout(central);
        state_label = new QLabel(central);
        state_label->setMinimumHeight(38);
        state_label->setAlignment(Qt::AlignCenter);
        root->addWidget(state_label);

        conversation = new QTextBrowser(central);
        conversation->setOpenExternalLinks(false);
        root->addWidget(conversation, 1);

        progress = new QProgressBar(central);
        progress->setRange(0, 1);
        progress->setValue(0);
        progress->setFormat(MainWindow::tr("问题进度：%v / %m"));
        root->addWidget(progress);

        auto* text_row = new QHBoxLayout();
        text_answer = new QTextEdit(central);
        text_answer->setPlaceholderText(MainWindow::tr("文本模式下在这里输入回答"));
        text_answer->setMaximumHeight(90);
        submit_text_button = new QPushButton(MainWindow::tr("提交文本"), central);
        text_row->addWidget(text_answer, 1);
        text_row->addWidget(submit_text_button);
        root->addLayout(text_row);

        auto* report_row = new QHBoxLayout();
        report_label = new QLabel(MainWindow::tr("报告：尚未生成"), central);
        open_report_button = new QPushButton(MainWindow::tr("打开报告目录"), central);
        open_report_button->setEnabled(false);
        report_row->addWidget(report_label, 1);
        report_row->addWidget(open_report_button);
        root->addLayout(report_row);

        owner.setCentralWidget(central);
        owner.statusBar()->showMessage(MainWindow::tr("就绪"));

        QObject::connect(new_action, &QAction::triggered, [&]() { NewInterview(); });
        QObject::connect(new_button, &QPushButton::clicked, [&]() { NewInterview(); });
        QObject::connect(stop_action, &QAction::triggered, [&]() {
            QMetaObject::invokeMethod(worker, &InterviewWorker::StopInterview, Qt::QueuedConnection);
        });
        QObject::connect(start_record_button, &QPushButton::clicked, [&]() {
            QMetaObject::invokeMethod(worker, &InterviewWorker::BeginAudioAnswer, Qt::QueuedConnection);
        });
        QObject::connect(stop_record_button, &QPushButton::clicked, [&]() {
            QMetaObject::invokeMethod(worker, &InterviewWorker::EndAudioAnswer, Qt::QueuedConnection);
        });
        QObject::connect(submit_text_button, &QPushButton::clicked, [&]() {
            const QString answer = text_answer->toPlainText().trimmed();
            if (answer.isEmpty()) {
                return;
            }
            text_answer->clear();
            QMetaObject::invokeMethod(
                worker,
                [this, answer]() { worker->SubmitTextAnswer(answer); },
                Qt::QueuedConnection);
        });
        QObject::connect(open_report_button, &QPushButton::clicked, [&]() {
            if (!report_path.isEmpty()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(report_path).absolutePath()));
            }
        });
    }

    void ConnectWorker() {
        QObject::connect(worker, &InterviewWorker::StateChanged, &owner,
            [this](GuiInterviewState state) { UpdateState(state); }, Qt::QueuedConnection);
        QObject::connect(worker, &InterviewWorker::ActionReady, &owner,
            [this](const GuiInterviewAction& action) { ShowAction(action); }, Qt::QueuedConnection);
        QObject::connect(worker, &InterviewWorker::TranscriptReady, &owner,
            [this](const QString& text) { AppendMessage(MainWindow::tr("候选人"), text, "#087f5b"); },
            Qt::QueuedConnection);
        QObject::connect(worker, &InterviewWorker::ReportReady, &owner,
            [this](const QString& path) {
                report_path = path;
                report_label->setText(MainWindow::tr("报告：%1").arg(path));
                open_report_button->setEnabled(true);
            }, Qt::QueuedConnection);
        QObject::connect(worker, &InterviewWorker::ErrorOccurred, &owner,
            [this](const QString& error) {
                AppendMessage(MainWindow::tr("系统"), error, "#c92a2a");
                QMessageBox::critical(&owner, MainWindow::tr("面试错误"), error);
            }, Qt::QueuedConnection);
    }

    void NewInterview() {
        ConfigDialog dialog(&owner);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        current_config = dialog.InterviewConfig();
        conversation->clear();
        report_path.clear();
        report_label->setText(MainWindow::tr("报告：尚未生成"));
        open_report_button->setEnabled(false);
        progress->setRange(0, current_config.min_questions);
        progress->setValue(0);
        QMetaObject::invokeMethod(
            worker,
            [this, config = current_config]() { worker->StartInterview(config); },
            Qt::QueuedConnection);
    }

    void ShowAction(const GuiInterviewAction& action) {
        const QString role = action.type == core::InterviewActionType::kFinish
            ? MainWindow::tr("面试总结") : MainWindow::tr("面试官");
        AppendMessage(role, action.text, "#1864ab");
        if (action.question_index > 0) {
            progress->setValue(action.question_index);
        }
    }

    void AppendMessage(const QString& role, const QString& text, const char* color) {
        conversation->append(QStringLiteral("<p><b style='color:%1'>%2：</b> %3</p>")
            .arg(QString::fromLatin1(color), role.toHtmlEscaped(), text.toHtmlEscaped()));
    }

    void UpdateState(GuiInterviewState state) {
        const QString text = StateText(state);
        state_label->setText(text);
        owner.statusBar()->showMessage(text);
        const bool waiting = state == GuiInterviewState::kWaitingForAnswer;
        start_record_button->setEnabled(waiting && current_config.audio_mode);
        stop_record_button->setEnabled(state == GuiInterviewState::kRecording);
        text_answer->setEnabled(waiting && !current_config.audio_mode);
        submit_text_button->setEnabled(waiting && !current_config.audio_mode);
        new_button->setEnabled(state == GuiInterviewState::kReady ||
                               state == GuiInterviewState::kCompleted ||
                               state == GuiInterviewState::kError);

        QString color = "#495057";
        if (state == GuiInterviewState::kRecording) color = "#2b8a3e";
        if (state == GuiInterviewState::kError) color = "#c92a2a";
        if (state == GuiInterviewState::kCompleted) color = "#1864ab";
        state_label->setStyleSheet(QStringLiteral(
            "QLabel{font-weight:600;padding:8px;border:1px solid #dee2e6;"
            "border-radius:4px;color:%1;background:#f8f9fa;}").arg(color));
    }

    MainWindow& owner;
    QThread worker_thread;
    InterviewWorker* worker = nullptr;
    GuiInterviewConfig current_config;
    QString report_path;
    QLabel* state_label = nullptr;
    QTextBrowser* conversation = nullptr;
    QProgressBar* progress = nullptr;
    QPushButton* new_button = nullptr;
    QPushButton* start_record_button = nullptr;
    QPushButton* stop_record_button = nullptr;
    QTextEdit* text_answer = nullptr;
    QPushButton* submit_text_button = nullptr;
    QLabel* report_label = nullptr;
    QPushButton* open_report_button = nullptr;
};

MainWindow::MainWindow(config::AppConfig config, QWidget* parent)
    : QMainWindow(parent), pImpl(std::make_unique<Impl>(*this, std::move(config))) {}
MainWindow::~MainWindow() = default;

} // namespace gui
} // namespace interview
