#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>


// 包含你之前实现的 date_folder_rotating_sink
#include "logger/date_folder_rotating_sink.h"

int main()
{
  try
  {
    // 创建自定义 sink
    auto sink = std::make_shared<date_folder_rotating_sink_mt>("./logs", "app.log", 1024 * 1024, 3);

    // 设置日志格式
    sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    // 设置日志级别
    sink->set_level(spdlog::level::info);
    // 数字越大越旧，当前文件永远是无后缀的日志文件。
    sink->set_max_size(1024);  // 1kB
    sink->set_max_files(10);  // 10 files

    // 创建 logger
    auto logger = std::make_shared<spdlog::logger>("my_logger", sink);

    spdlog::register_logger(logger);

    // 打印不同级别日志
    logger->trace("这是一条 trace 日志");  // 不会输出，因为 level = info
    logger->debug("这是一条 debug 日志");  // 不会输出
    logger->info("这是一条 info 日志");    // 会输出
    logger->warn("这是一条 warn 日志");    // 会输出
    logger->error("这是一条 error 日志");  // 会输出

    // 模拟多条日志输出
    for (int i = 0; i < 10; ++i)
    {
      logger->info("日志测试条目 #{}", i);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 刷新日志
    logger->flush();

    spdlog::drop("my_logger");  // 卸载 logger
  }
  catch (const spdlog::spdlog_ex &ex)
  {
    std::cerr << "Log 初始化失败: " << ex.what() << std::endl;
  }

  return 0;
}
