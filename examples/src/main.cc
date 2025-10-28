#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "logger/log_manager.h"

// 包含你之前实现的 date_folder_rotating_sink
#include "logger/date_folder_rotating_sink.h"

int main()
{
  try
  {
    auto logger = LogManager::getLogger("my_logger");

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
  }
  catch (const spdlog::spdlog_ex &ex)
  {
    std::cerr << "Log 初始化失败: " << ex.what() << std::endl;
  }

  return 0;
}
