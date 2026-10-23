# 这部分负责收集core下的源码，并进行构建
file(GLOB AI_INTERVIEW_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"    
)

add_library(ai_interview_core
    ${AI_INTERVIEW_CORE_SOURCES}
)

# 从项目源目录下收集include下的头文件
target_include_directories(ai_interview_core PUBLIC 
    ${PROJECT_SOURCE_DIR}/include        
)

# 声明 ai_interview_core 至少需要 C++17
# PUBLIC 表示链接该库的目标也会继承 C++17 要求，
# 避免测试或上层模块用低于 C++17 的标准编译
target_compile_features(ai_interview_core
    PUBLIC
        cxx_std_17
)

# 为不同编译器设置较严格的编译警告。
# MSVC 使用 Windows/MSVC 风格参数：
# /W4         开启较高等级警告
# /permissive- 尽量按标准 C++ 编译，减少 MSVC 扩展行为
# /utf-8      按 UTF-8 处理源码，避免中文注释或字符串乱码
#
# GCC/Clang 使用 Unix 风格参数：
# -Wall      开启常见警告
# -Wextra    开启额外警告
# -Wpedantic 按标准 C++ 检查不规范写法
if(MSVC) 
    target_compile_options(ai_interview_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ai_interview_core PRIVATE -Wall -Wextra -Wpedantic)
endif()