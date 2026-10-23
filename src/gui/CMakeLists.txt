find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(ai_interview_gui
    ../app/gui_main.cpp
    interview_worker.cpp
    config_dialog.cpp
    main_window.cpp
    ${PROJECT_SOURCE_DIR}/include/gui/gui_models.h
    ${PROJECT_SOURCE_DIR}/include/gui/interview_worker.h
    ${PROJECT_SOURCE_DIR}/include/gui/config_dialog.h
    ${PROJECT_SOURCE_DIR}/include/gui/main_window.h
)

set_target_properties(ai_interview_gui PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "AI Interview"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.aiinterview.desktop"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/packaging/macos/Info.plist.in"
)

target_include_directories(ai_interview_gui PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(ai_interview_gui PRIVATE
    ai_interview_config
    ai_interview_runtime
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
target_compile_features(ai_interview_gui PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(ai_interview_gui PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ai_interview_gui PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS ai_interview_gui
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

if(COMMAND qt_generate_deploy_app_script)
    qt_generate_deploy_app_script(
        TARGET ai_interview_gui
        OUTPUT_SCRIPT ai_interview_qt_deploy_script
        NO_TRANSLATIONS
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${ai_interview_qt_deploy_script})
endif()

if(WIN32)
    get_target_property(QT_QMAKE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ai_interview_gui POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --compiler-runtime
                "$<TARGET_FILE:ai_interview_gui>"
            COMMENT "Deploying Qt runtime for Windows"
        )
    endif()
elseif(APPLE)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt)
    if(MACDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ai_interview_gui POST_BUILD
            COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                "$<TARGET_BUNDLE_DIR:ai_interview_gui>"
                -always-overwrite
            COMMENT "Deploying Qt frameworks for macOS"
        )
    endif()
elseif(UNIX)
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/linux/ai-interview.desktop"
        DESTINATION share/applications)
endif()
