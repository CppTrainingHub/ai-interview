#include <cassert>

#include <QApplication>
#include <QSignalSpy>

#include "gui/interview_worker.h"
#include "gui/main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    interview::config::AppConfig config;
    config.dependency_mode = "scripted";
    config.runtime_mode = "text";

    interview::gui::InterviewWorker worker(config);
    QSignalSpy actions(&worker, &interview::gui::InterviewWorker::ActionReady);
    QSignalSpy states(&worker, &interview::gui::InterviewWorker::StateChanged);

    interview::gui::GuiInterviewConfig gui_config;
    gui_config.candidate_name = QStringLiteral("GUI Test");
    gui_config.min_questions = 1;
    gui_config.audio_mode = false;
    worker.StartInterview(gui_config);
    assert(actions.count() == 1);
    assert(states.count() >= 2);

    worker.SubmitTextAnswer(QStringLiteral("我是 GUI 测试候选人"));
    assert(actions.count() == 2);
    worker.StopInterview();

    interview::gui::MainWindow window(config);
    window.show();
    app.processEvents();
    assert(window.isVisible());
    window.close();
    return 0;
}
