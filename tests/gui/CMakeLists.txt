find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test)

add_executable(gui_module_test
    gui_module_test.cpp
    ${PROJECT_SOURCE_DIR}/src/gui/interview_worker.cpp
    ${PROJECT_SOURCE_DIR}/src/gui/config_dialog.cpp
    ${PROJECT_SOURCE_DIR}/src/gui/main_window.cpp
    ${PROJECT_SOURCE_DIR}/include/gui/interview_worker.h
    ${PROJECT_SOURCE_DIR}/include/gui/config_dialog.h
    ${PROJECT_SOURCE_DIR}/include/gui/main_window.h
)
set_target_properties(gui_module_test PROPERTIES AUTOMOC ON)
target_include_directories(gui_module_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(gui_module_test PRIVATE
    ai_interview_config
    ai_interview_runtime
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)
target_compile_features(gui_module_test PRIVATE cxx_std_17)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(XVFB_RUN_EXECUTABLE xvfb-run)
    if(XVFB_RUN_EXECUTABLE)
        add_test(NAME gui_module_test
            COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:gui_module_test>)
        set_tests_properties(gui_module_test PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=xcb"
        )
    else()
        message(WARNING
            "xvfb-run was not found; gui_module_test is disabled because Qt's "
            "Linux xcb plugin requires a display server")
    endif()
else()
    add_test(NAME gui_module_test COMMAND gui_module_test)
endif()
