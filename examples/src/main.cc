#include <spdlog/spdlog.h>

#include <iostream>
#include <memory>

#include "logger/date_folder_rotating_sink.h"


int main()
{
  std::cout << "Logger Example" << std::endl;

  // -------------------------------
  // 1️⃣ 创建自定义 sink（每天文件夹 + rotating）
  auto sink = std::make_shared<date_folder_rotating_sink_mt>("./logs",         // 日志根目录
                                                             "myapp.log",      // 文件名
                                                             5 * 1024 * 1024,  // max_size
                                                             3                 // max_files
  );

  // -------------------------------
  // 2️⃣ 创建 logger，并设置默认 logger
  auto logger = std::make_shared<spdlog::logger>("daily_logger", sink);

  // 给 logger 设置 pattern，这样 sink 的 pattern 会生效
  logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");

  // 注册 logger（可选）
  spdlog::register_logger(logger);

  // 设置为默认 logger，这样 spdlog::info() 会使用它
  spdlog::set_default_logger(logger);

  // -------------------------------
  // 3️⃣ 写日志
  for (int i = 0; i < 100; ++i)
  {
    spdlog::info("Test log {}", i);
  }

  // 自动 flush
  spdlog::flush_on(spdlog::level::info);

  return 0;
}
