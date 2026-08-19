cmake_minimum_required(VERSION 3.20)
project(CppInterviewSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt支持
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# vcpkg集成
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# 平台检测
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
    set(PLATFORM_NAME "Windows")
elseif(UNIX)
    add_definitions(-DPLATFORM_LINUX)
    set(PLATFORM_NAME "Linux")
endif()

message(STATUS "Building for platform: ${PLATFORM_NAME}")

# 查找依赖库
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(portaudio CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(podofo CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Qt依赖（必需，vcpkg的qtbase是Qt6）
find_package(Qt6 COMPONENTS Core Widgets Gui QUIET)
if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    message(STATUS "Found Qt6")
else()
    # 尝试Qt5
    find_package(Qt5 COMPONENTS Core Widgets Gui QUIET)
    if(Qt5_FOUND)
        set(QT_VERSION_MAJOR 5)
        message(STATUS "Found Qt5")
    else()
        message(FATAL_ERROR "Qt not found! Please install Qt via vcpkg: vcpkg install qtbase")
    endif()
endif()

set(COMMON_SOURCES
    src/common/logger.cpp
    src/common/config.cpp
    src/common/protocol.cpp
    src/common/interview_state.cpp
)
set(SERVICES_SOURCES
    src/services/realtime_client.cpp
    src/services/audio_manager.cpp
    src/services/llm_client.cpp
    src/services/pdf_parser.cpp
)
set(INTERVIEW_SOURCES
    src/interview/interview_manager.cpp
    src/interview/dialog_session.cpp
)
set(UI_SOURCES
    src/ui/mainwindow.cpp
    src/ui/config_dialog.cpp
)
set(UI_HEADERS
    include/ui/mainwindow.h
    include/ui/config_dialog.h
)

set(MAIN_SOURCES
    src/main_qt.cpp
)

set(SOURCES
    ${COMMON_SOURCES}
    ${SERVICES_SOURCES}
    ${INTERVIEW_SOURCES}
    ${UI_SOURCES}
    ${UI_HEADERS}
    ${MAIN_SOURCES}
)

set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/common
    ${CMAKE_CURRENT_SOURCE_DIR}/include/services
    ${CMAKE_CURRENT_SOURCE_DIR}/include/interview
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${INCLUDE_DIRS}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    AUTOMOC_MOC_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/include"
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/include/ui"
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE ON
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    # OpenSSL
    OpenSSL::SSL
    OpenSSL::Crypto
    # Boost
    Boost::system
    Boost::thread
    # JSON
    nlohmann_json::nlohmann_json
    # 压缩
    ZLIB::ZLIB
    # 音频
    portaudio
    # HTTP客户端
    CURL::libcurl
    # PDF解析
    podofo::podofo
    # 日志
    spdlog::spdlog
    # Qt界面
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 wsock32)
endif()

if(MSVC)
    # MSVC编译器选项
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4           # 警告级别4
        /utf-8        # UTF-8编码
        /permissive-  # 严格标准模式
        /Zc:__cplusplus  # 正确设置__cplusplus宏
        $<$<CONFIG:Release>:/O2 /GL>  # Release优化
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/LTCG>  # 链接时代码生成
    )
else()
    # GCC/Clang编译器选项
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3 -march=native -flto>
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:-flto>
    )
endif()

if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
    
    if(WINDEPLOYQT_EXECUTABLE)
        message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE}
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                $<TARGET_FILE:${PROJECT_NAME}>
            COMMENT "Running windeployqt to deploy Qt dependencies..."
        )
    else()
        message(WARNING "windeployqt not found. You will need to manually deploy Qt dependencies.")
    endif()
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
