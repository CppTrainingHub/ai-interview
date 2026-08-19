#pragma once

#include <QMainWindow>
#include <QThread>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <memory>

#include "common/interview_state.h"

namespace interview {
namespace session {
    class DialogSession;
}

namespace ui {

using common::InterviewState;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void OnNewSession();
    void OnStartSession();
    
    // 状态机回调触发的UI更新
    void OnStateChangedFromMachine(InterviewState old_state, InterviewState new_state);

private:
    void SetupUi();
    void SetupMenuBar();
    void SetupToolBar();
    void SetupCentralWidget();
    void UpdateUIState(InterviewState state);
    QString GetStateText(InterviewState state);
    void AppendMessage(const QString& text, const QString& color = "black");
    
    QTextEdit* message_area_;
    QLabel* status_label_;
    QProgressBar* progress_bar_;
    QPushButton* start_button_;
    QPushButton* new_session_button_;
    
    std::unique_ptr<session::DialogSession> session_;
    std::thread session_thread_;
    
    QString candidate_name_;
    QString resume_path_;
    int min_questions_;
};

} // namespace ui
} // namespace interview

