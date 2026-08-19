#include <QApplication>
#include "ui/mainwindow.h"
#include "common/logger.h"
#include "common/config.h"
#include <iostream>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // 设置应用信息
    QApplication::setApplicationName("C++ Interview System");
    QApplication::setApplicationVersion("1.0");
    QApplication::setOrganizationName("Interview");
    
    // 初始化日志系统
    interview::common::Logger::Init("interview_qt.log", true);
    
    LOG_INFO("========================================");
    LOG_INFO("C++ Interview System (Qt GUI) v1.0");
    LOG_INFO("========================================");
    
    // 加载配置文件
    try {
        auto& cfg = interview::common::Config::Instance();
        
        // 尝试从多个位置加载配置
        std::vector<std::string> config_paths = {
            "config/default_config.json",
            "../config/default_config.json",
            "../../config/default_config.json"
        };
        
        bool config_loaded = false;
        for (const auto& path : config_paths) {
            try {
                cfg.LoadFromFile(path);
                LOG_INFO("Configuration loaded from: {}", path);
                config_loaded = true;
                break;
            } catch (...) {
                continue;
            }
        }
        
        if (!config_loaded) {
            LOG_WARNING("Could not load config file, using default values");
            std::cerr << "警告: 无法加载配置文件，将使用默认配置" << std::endl;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Configuration error: {}", e.what());
        std::cerr << "配置错误: " << e.what() << std::endl;
    }
    
    // 创建并显示主窗口
    interview::ui::MainWindow window;
    window.show();
    
    LOG_INFO("Qt GUI started");
    
    // 运行应用
    int result = app.exec();
    
    LOG_INFO("Application exiting with code: {}", result);
    return result;
}

