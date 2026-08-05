# 指定CMake最低版本要求
cmake_minimum_required(VERSION 3.15)

# 生成 compile_commands.json 文件（用于 clangd、clang-tidy 等工具）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 构建类型默认 Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release 
        CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS 
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# 设置C++标准版本为C++17，并确保严格要求此标准，不使用扩展功能
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置所有库生成位置无关代码 (Position Independent Code) 对应GCC/Clang的-fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 设置输出路径：库文件、可执行文件和归档文件的生成目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/build_output/lib) # 静态库 (.a/.lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/build_output/bin) # 动态库 (.so/.dll)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/build_output/bin) # 可执行文件

# 设置位置无关代码 (PIC)，对于动态库来说是必须的
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 项目名称和语言
project(mlog 
    VERSION 0.1.0
    LANGUAGES C CXX
    DESCRIPTION "Modern logging library"
)

# 仅在 Linux/macOS 上启用 pthread
if(UNIX)
    find_package(Threads REQUIRED)
    set(THREAD_LIB Threads::Threads)
else()
    set(THREAD_LIB "") # Windows 不需要 pthread
endif()

# 添加子目录
# 或者根据 C++ 版本条件添加
if(CMAKE_CXX_STANDARD LESS 17)
    add_subdirectory(3rd/filesystem)
endif()
add_subdirectory(3rd/spdlog)

add_subdirectory(examples)
add_subdirectory(logger)
