#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>

#include "logger/log_manager.h"
#include "spdlog/sinks/basic_file_sink.h"

int main()
{
  try
  {
#if defined(_WIN32)
    std::system("chcp 65001");  // 设置控制台为 UTF-8 编码
#endif
    auto logger = LogManager::get_logger("module1");
    logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] [%n] %v");
    // 注意logger和sink的pattern是分开的, logger会覆盖sink的pattern
    logger->sinks()[0]->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    auto logger2 = LogManager::get_logger("module2");
    logger2->set_level(spdlog::level::trace);              // logger2的日志级别设置为 trace, 这个会优先过滤
    logger2->sinks()[0]->set_level(spdlog::level::trace);  // 设置 logger2 的文件 sink 日志级别为 trace
    logger2->sinks()[1]->set_level(spdlog::level::info);   // 设置 logger2 的文件 sink 日志级别为 info

    // LogManager::setStdoutGlobalLevel(spdlog::level::info);  // 设置控制台日志级别为 info
    // LogManager::setFileGlobalLevel(spdlog::level::warn);    // 设置文件日志级别为 warn

    // 打印不同级别日志
    logger->trace("这是一条 trace 日志");  // 不会输出，因为 level = info
    logger->debug("这是一条 debug 日志");  // 不会输出
    logger->info("这是一条 info 日志");    // 会输出
    logger->warn("这是一条 warn 日志");    // 会输出
    logger->error("这是一条 error 日志");  // 会输出

    logger2->trace("这是一条 trace 日志");  // 不会输出，因为 level = info
    logger2->debug("这是一条 debug 日志");  // 不会输出
    logger2->info("这是一条 info 日志");    // 会输出
    logger2->warn("这是一条 warn 日志");    // 会输出
    logger2->error("这是一条 error 日志");  // 会输出

    // 模拟多条日志输出
    for (int i = 0; i < 10; ++i)
    {
      logger->info("日志测试条目 #{}", i);
      logger2->info("日志测试条目 #{}", i);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // spdlog存在缓存机制, 需要手动 flush 才能确保日志写入文件
    logger->flush();
    // 每s刷新已注册的日志
    spdlog::flush_every(std::chrono::seconds(1));

    // 测试添加已有 logger
    auto another_logger = spdlog::basic_logger_mt("basic_logger", "logs/basicfile.log");
    // auto another_logger = std::make_shared<spdlog::logger>("basic_logger",
    // std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/basicfile.txt"));
    LogManager::add_logger(another_logger);
    another_logger->info("这是 basic_logger 的一条日志");
    another_logger->warn("这是 basic_logger 的一条警告日志");
    another_logger->error("这是 basic_logger 的一条错误日志");
    LogManager::flush_all();
  }
  catch (const spdlog::spdlog_ex &ex)
  {
    std::cerr << "Log 初始化失败: " << ex.what() << '\n';
  }

  return 0;
}
