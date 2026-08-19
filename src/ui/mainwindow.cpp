#include "ui/mainwindow.h"
#include "ui/config_dialog.h"
#include "interview/dialog_session.h"
#include "common/logger.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QTextOption>
#include <thread>

namespace interview {
namespace ui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , candidate_name_("候选人")
    , min_questions_(15) {
    
    SetupUi();
    SetupMenuBar();
    SetupToolBar();
    SetupCentralWidget();
    
    // 注册全局状态机回调
    common::InterviewStateMachine::Instance().SetStateChangeCallback(
        [this](common::InterviewState old_state, common::InterviewState new_state) {
            // 在Qt主线程中更新UI
            QMetaObject::invokeMethod(this, [this, old_state, new_state]() {
                OnStateChangedFromMachine(old_state, new_state);
            }, Qt::QueuedConnection);
        }
    );
    
    setWindowTitle("C++ 语音面试系统");
    resize(1000, 700);
    
    UpdateUIState(common::InterviewStateMachine::Instance().GetState());
}

MainWindow::~MainWindow() {
    // 取消注册状态机回调
    common::InterviewStateMachine::Instance().ClearStateChangeCallback();
    
    if (session_ && session_->IsRunning()) {
        session_->Stop();
    }
    
    if (session_thread_.joinable()) {
        session_thread_.join();
    }
}

void MainWindow::SetupUi() {
    // 创建中央widget
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
}

void MainWindow::SetupMenuBar() {
    QMenuBar* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);
    
    // 文件菜单
    QMenu* fileMenu = menuBar->addMenu("文件(&F)");
    QAction* newAction = fileMenu->addAction("新建会话(&N)");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::OnNewSession);
    
    fileMenu->addSeparator();
    
    QAction* exitAction = fileMenu->addAction("退出(&X)");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    // 帮助菜单
    QMenu* helpMenu = menuBar->addMenu("帮助(&H)");
    QAction* aboutAction = helpMenu->addAction("关于(&A)");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "关于",
            "C++ 语音面试系统 v2.0 \n\n"
            "基于AI的实时语音技术面试平台\n"
            "支持简历驱动问题生成和智能评分\n\n");
    });
}

void MainWindow::SetupToolBar() {
    QToolBar* toolbar = addToolBar("主工具栏");
    toolbar->setMovable(false);
    
    new_session_button_ = new QPushButton("新建会话", this);
    start_button_ = new QPushButton("开始面试", this);
    
    toolbar->addWidget(new_session_button_);
    toolbar->addSeparator();
    toolbar->addWidget(start_button_);
    toolbar->addSeparator();
    
    connect(new_session_button_, &QPushButton::clicked, this, &MainWindow::OnNewSession);
    connect(start_button_, &QPushButton::clicked, this, &MainWindow::OnStartSession);
}

void MainWindow::SetupCentralWidget() {
    QWidget* central = centralWidget();
    QVBoxLayout* layout = new QVBoxLayout(central);
    
    // 状态栏
    status_label_ = new QLabel("就绪", this);
    status_label_->setStyleSheet(
        "QLabel { "
        "  padding: 8px 12px; "
        "  font-weight: bold; "
        "  font-size: 12pt; "
        "  background-color: #f5f5f5; "
        "  border: 1px solid #ccc; "
        "  color: #333; "
        "}"
    );
    layout->addWidget(status_label_);
    
    // 消息显示区域
    message_area_ = new QTextEdit(this);
    message_area_->setReadOnly(true);
    message_area_->setLineWrapMode(QTextEdit::WidgetWidth);
    message_area_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    message_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    message_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    message_area_->setStyleSheet(
        "QTextEdit { "
        "  background-color: #ffffff; "
        "  border: 1px solid #ddd; "
        "  padding: 10px; "
        "  font-family: 'Microsoft YaHei', sans-serif; "
        "  font-size: 11pt; "
        "  line-height: 1.6; "
        "}"
        "QScrollBar:vertical {"
        "  border: none;"
        "  background: #f0f0f0;"
        "  width: 10px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #c0c0c0;"
        "  border-radius: 5px;"
        "  min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #999;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
    );
    layout->addWidget(message_area_, 1);
    
    // 进度条
    progress_bar_ = new QProgressBar(this);
    progress_bar_->setRange(0, 10);
    progress_bar_->setValue(0);
    progress_bar_->setTextVisible(true);
    progress_bar_->setFormat("进度: %v/%m 题");
    progress_bar_->setMinimumHeight(28);
    progress_bar_->setStyleSheet(
        "QProgressBar {"
        "  border: 1px solid #ddd;"
        "  background-color: #f0f0f0;"
        "  text-align: center;"
        "  font-weight: bold;"
        "  font-size: 10pt;"
        "  color: #333;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #4CAF50;"
        "}"
    );
    layout->addWidget(progress_bar_);
    
    // 状态栏
    statusBar()->showMessage("就绪");
}

void MainWindow::OnNewSession() {
    ConfigDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        candidate_name_ = dialog.GetCandidateName();
        resume_path_ = dialog.GetResumePath();
        min_questions_ = dialog.GetMinQuestions();
        
        message_area_->clear();
        progress_bar_->setValue(0);
        progress_bar_->setMaximum(min_questions_);
        
        // 配置信息显示
        QString config_info = QString("===== 会话配置 =====\n")
                             + QString("候选人: %1\n").arg(candidate_name_);
        if (!resume_path_.isEmpty()) {
            config_info += QString("简历: %1\n").arg(resume_path_);
        }
        config_info += QString("问题数量: %1 题\n").arg(min_questions_);
        config_info += QString("========================\n");
        
        AppendMessage(config_info);
        
        start_button_->setEnabled(true);
        statusBar()->showMessage("会话已配置，点击'开始面试'启动");
    }
}

void MainWindow::OnStartSession() {
    if (candidate_name_.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先新建会话并配置");
        return;
    }
    
    // 禁用按钮，防止重复点击
    start_button_->setEnabled(false);
    new_session_button_->setEnabled(false);
    
    // 根据是否有简历显示不同状态
    if (!resume_path_.isEmpty()) {
        status_label_->setText("● 解析简历，生成问题中");
        status_label_->setStyleSheet(
            "QLabel { "
            "  padding: 8px 12px; "
            "  font-weight: bold; "
            "  font-size: 12pt; "
            "  background-color: #fff3e0; "
            "  border: 1px solid #ffb74d; "
            "  border-radius: 4px; "
            "  color: #ff9800; "
            "}"
        );
        AppendMessage("正在解析简历，生成问题中...\n", "blue");
        statusBar()->showMessage("正在解析简历，生成问题中...");
    } else {
        status_label_->setText("● 生成问题中");
        status_label_->setStyleSheet(
            "QLabel { "
            "  padding: 8px 12px; "
            "  font-weight: bold; "
            "  font-size: 12pt; "
            "  background-color: #fff3e0; "
            "  border: 1px solid #ffb74d; "
            "  border-radius: 4px; "
            "  color: #ff9800; "
            "}"
        );
        AppendMessage("正在生成问题中...\n", "blue");
        statusBar()->showMessage("正在生成问题中...");
    }
    
    // 启动会话线程
    session_thread_ = std::thread([this]() {
        try {
            session_ = std::make_unique<session::DialogSession>(candidate_name_.toStdString());
            
            // 设置对话内容回调
            session_->SetDialogContentCallback(
                [this](const std::string& role, const std::string& text, int question_index) {
                    // 切换到主线程更新UI
                    QMetaObject::invokeMethod(this, [this, role, text, question_index]() {
                        if (role == "interviewer") {
                            AppendMessage(QString("🎙️ 【面试官】：%1\n").arg(QString::fromStdString(text)), "#0066cc");
                            progress_bar_->setValue(question_index);
                        } else if (role == "candidate") {
                            AppendMessage(QString("👤 【候选人】：%1\n").arg(QString::fromStdString(text)), "#009688");
                        }
                    }, Qt::QueuedConnection);
                }
            );
            
            // 配置简历面试
            if (!resume_path_.isEmpty()) {
                session_->ConfigureResumeInterview(resume_path_.toStdString(), min_questions_);
            } else {
                session_->ConfigureDefaultInterview(min_questions_); 
            }
            
            // 启动会话
            session_->Start();
            
        } catch (const std::exception& e) {
            // 在主线程显示错误
            QMetaObject::invokeMethod(this, [this, error = std::string(e.what())]() {
                QMessageBox::critical(this, "错误", QString::fromStdString(error));
                AppendMessage(QString("错误: %1\n").arg(QString::fromStdString(error)), "red");
                
                // 恢复按钮状态
                start_button_->setEnabled(true);
                new_session_button_->setEnabled(true);
            }, Qt::QueuedConnection);
        }
    });
}

void MainWindow::OnStateChangedFromMachine(InterviewState old_state, InterviewState new_state) {
    UpdateUIState(new_state);
    
    QString state_text = GetStateText(new_state);
    status_label_->setText("● " + state_text);
    statusBar()->showMessage(state_text);
    
    LOG_INFO("UI State changed: {} -> {}", 
             common::InterviewStateMachine::Instance().GetStateName(old_state),
             common::InterviewStateMachine::Instance().GetStateName(new_state));
    
    // 1. 连接成功 -> 面试官说话（开场白）
    if (old_state == InterviewState::kConnecting && new_state == InterviewState::kInterviewerSpeaking) {
        AppendMessage("\n✓ 连接成功，面试开始\n", "blue");
    }
    // 2. 空闲 -> 面试官说话（提问）
    else if (old_state == InterviewState::kIdle && new_state == InterviewState::kInterviewerSpeaking) {
        AppendMessage("\n【面试官提问】\n", "blue");
    }
    // 3. 面试官思考 -> 面试官说话（下一个问题或追问）
    else if (old_state == InterviewState::kInterviewerThinking && new_state == InterviewState::kInterviewerSpeaking) {
        AppendMessage("\n【面试官提问】\n", "blue");
    }
    // 4. 空闲 -> 候选人说话（VAD检测到说话）
    else if (old_state == InterviewState::kIdle && new_state == InterviewState::kCandidateSpeaking) {
        AppendMessage("\n【候选人回答中】\n", "green");
    }
    // 5. 候选人说话 -> 面试官思考（ASR最终结果）
    else if (old_state == InterviewState::kCandidateSpeaking && new_state == InterviewState::kInterviewerThinking) {
        AppendMessage("【回答结束，正在评估...】\n", "orange");
    }
    // 6. 会话结束
    else if (new_state == InterviewState::kSessionEnding) {
        AppendMessage("\n===== 面试即将结束 =====\n", "blue");
    }
    else if (new_state == InterviewState::kCompleted) {
        AppendMessage("\n✓ 面试已完成！报告已保存\n", "blue");
        QMessageBox::information(this, "完成", "面试已完成！报告已保存。");
    }
    else if (new_state == InterviewState::kError) {
        AppendMessage("\n✗ 发生错误，面试中断\n", "red");
    }
}

void MainWindow::UpdateUIState(InterviewState state) {
    QString baseStyle = "QLabel { padding: 8px 12px; font-weight: bold; font-size: 12pt; "
                       "background-color: #fff; border: 1px solid #ddd; border-radius: 4px; ";
    
    switch (state) {
    case InterviewState::kIdle:
        if (session_ && session_->IsRunning()) {
            start_button_->setEnabled(false);
            new_session_button_->setEnabled(false);
            status_label_->setStyleSheet(baseStyle + "color: #4caf50; background-color: #e8f5e9; border-color: #81c784; }");
        } else {
            start_button_->setEnabled(false);
            new_session_button_->setEnabled(true);
            status_label_->setStyleSheet(baseStyle + "color: #666; }");
        }
        break;
    case InterviewState::kConnecting:
        start_button_->setEnabled(false);
        new_session_button_->setEnabled(false);
        status_label_->setStyleSheet(baseStyle + "color: #ff9800; background-color: #fff3e0; border-color: #ffb74d; }");
        break;
    case InterviewState::kInterviewerSpeaking:
    case InterviewState::kCandidateSpeaking:
    case InterviewState::kInterviewerThinking:
        start_button_->setEnabled(false);
        new_session_button_->setEnabled(false);
        status_label_->setStyleSheet(baseStyle + "color: #4caf50; background-color: #e8f5e9; border-color: #81c784; }");
        break;
    case InterviewState::kSessionEnding:
        start_button_->setEnabled(false);
        new_session_button_->setEnabled(false);
        status_label_->setStyleSheet(baseStyle + "color: #ff9800; background-color: #fff3e0; border-color: #ffb74d; }");
        break;
    case InterviewState::kCompleted:
        start_button_->setEnabled(false);
        new_session_button_->setEnabled(true);
        status_label_->setStyleSheet(baseStyle + "color: #2196f3; background-color: #e3f2fd; border-color: #64b5f6; }");
        break;
    case InterviewState::kError:
        start_button_->setEnabled(false);
        new_session_button_->setEnabled(true);
        status_label_->setStyleSheet(baseStyle + "color: #f44336; background-color: #ffebee; border-color: #e57373; }");
        break;
    default:
        break;
    }
}

QString MainWindow::GetStateText(InterviewState state) {
    return QString::fromUtf8(common::InterviewStateMachine::Instance().GetStateName(state));
}

void MainWindow::AppendMessage(const QString& text, const QString& color) {
    QString html_text = QString("<span style='color: %1;'>%2</span><br>")
                        .arg(color)
                        .arg(text.toHtmlEscaped().replace("\n", "<br>"));
    
    message_area_->moveCursor(QTextCursor::End);
    message_area_->insertHtml(html_text);
    message_area_->moveCursor(QTextCursor::End);
    message_area_->ensureCursorVisible();
}

} // namespace ui
} // namespace interview
