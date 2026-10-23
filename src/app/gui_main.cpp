#include <exception>

#include <QApplication>
#include <QMessageBox>

#include "config/app_config.h"
#include "gui/main_window.h"

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("AI Interview"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setOrganizationName(QStringLiteral("AIInterview"));

    try {
        auto config = interview::config::LoadAppConfig();
        interview::gui::MainWindow window(std::move(config));
        window.show();
        return application.exec();
    } catch (const std::exception& exception) {
        QMessageBox::critical(nullptr, QObject::tr("启动失败"),
                              QString::fromUtf8(exception.what()));
        return 1;
    }
}
