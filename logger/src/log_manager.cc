#include "logger/log_manager.h"

#include <mutex>
#include <unordered_map>

#include "logger/date_folder_rotating_sink.h"

namespace
{
// 获取全局 logger 映射（局部静态变量，线程安全初始化）
std::unordered_map<std::string, std::shared_ptr<spdlog::logger>>& get_logger_map()
{
  static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> map;
  return map;
}

// 获取互斥锁（局部静态变量）
std::mutex& get_logger_mutex()
{
  static std::mutex mtx;
  return mtx;
}

// 全局默认日志级别（后续新建 logger 也继承这个级别）
spdlog::level::level_enum& get_default_level()
{
  static spdlog::level::level_enum level = spdlog::level::info;
  return level;
}
}  // namespace

std::shared_ptr<spdlog::logger> LogManager::getLogger(const std::string& module)
{
  auto& loggers = get_logger_map();
  auto& mtx = get_logger_mutex();

  {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = loggers.find(module);
    if (it != loggers.end()) return it->second;
  }

  // 未找到则加锁创建
  std::lock_guard<std::mutex> lock(mtx);
  auto it = loggers.find(module);
  if (it != loggers.end()) return it->second;

  // 创建日期文件夹滚动 sink
  auto sink = std::make_shared<date_folder_rotating_sink_mt>("./logs", module + ".log", 100 * 1024 * 1024, 10);

  sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
  sink->set_level(get_default_level());

  auto logger = std::make_shared<spdlog::logger>(module, sink);
  logger->set_level(get_default_level());
  spdlog::register_logger(logger);

  loggers[module] = logger;
  return logger;
}

void LogManager::setGlobalLevel(spdlog::level::level_enum level)
{
  auto& loggers = get_logger_map();
  auto& mtx = get_logger_mutex();
  get_default_level() = level;

  std::lock_guard<std::mutex> lock(mtx);
  for (auto& pair : loggers)
  {
    pair.second->set_level(level);
  }
}
