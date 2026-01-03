#include "logger/log_manager.h"

#include <spdlog/sinks/stdout_color_sinks.h>

#include <mutex>
#include <unordered_map>
#include <vector>

#include "logger/date_folder_rotating_sink.h"

namespace
{
// 局部静态资源
std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> &get_logger_map()
{
  static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> map;
  return map;
}

std::mutex &get_logger_mutex()
{
  static std::mutex mtx;
  return mtx;
}

spdlog::level::level_enum &get_default_file_level()
{
  static spdlog::level::level_enum level = spdlog::level::info;
  return level;
}

spdlog::level::level_enum &get_default_stdout_level()
{
  static spdlog::level::level_enum level = spdlog::level::warn;
  return level;
}

std::string &get_save_path()
{
  static std::string path = "./logs";
  return path;
}

}  // namespace

std::shared_ptr<spdlog::logger> LogManager::get_logger(const std::string &module)
{
  auto &loggers = get_logger_map();
  auto &mtx = get_logger_mutex();

  // 快速路径：短锁查询
  {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = loggers.find(module);
    if (it != loggers.end()) return it->second;
  }

  // 未找到则加锁创建
  std::lock_guard<std::mutex> lock(mtx);
  auto it = loggers.find(module);
  if (it != loggers.end()) return it->second;

  // === 文件 Sink（每天一个文件夹）===
  auto file_sink =
    std::make_shared<date_folder_rotating_sink_mt>(get_save_path(), module + ".log", 100 * 1024 * 1024, 10);
  file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
  file_sink->set_level(get_default_file_level());

  // === 控制台 Sink（带颜色）===
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
  console_sink->set_level(get_default_stdout_level());

  // === 创建 logger（同时绑定多个 sink）===
  std::vector<spdlog::sink_ptr> sinks{file_sink, console_sink};  // 顺序不能变, 文件 sink 在前
  auto logger = std::make_shared<spdlog::logger>(module, sinks.begin(), sinks.end());
  spdlog::register_logger(logger);

  loggers[module] = logger;
  return logger;
}

bool LogManager::add_logger(std::shared_ptr<spdlog::logger> logger)
{
  if (!logger) return false;

  auto &loggers = get_logger_map();
  auto &mtx = get_logger_mutex();

  std::lock_guard<std::mutex> lock(mtx);
  auto it = loggers.find(logger->name());
  if (it != loggers.end()) return false;  // 已存在同名 logger

  loggers[logger->name()] = logger;
  // 先检查 spdlog 内部是否已有同名 logger
  if (!spdlog::get(logger->name()))
  {
    spdlog::register_logger(logger);
  }
  return true;
}

void LogManager::set_file_global_level(spdlog::level::level_enum level)
{
  auto &loggers = get_logger_map();
  auto &mtx = get_logger_mutex();
  get_default_file_level() = level;

  std::lock_guard<std::mutex> lock(mtx);
  for (auto &pair : loggers)
  {
    if (pair.second->sinks().size() > 0)
    {
      pair.second->sinks()[0]->set_level(level);  // 第一个 sink 是文件 sink
    }
  }
}

void LogManager::set_stdout_global_level(spdlog::level::level_enum level)
{
  auto &loggers = get_logger_map();
  auto &mtx = get_logger_mutex();
  get_default_stdout_level() = level;

  std::lock_guard<std::mutex> lock(mtx);
  for (auto &pair : loggers)
  {
    if (pair.second->sinks().size() > 1)
    {
      pair.second->sinks()[1]->set_level(level);  // 第二个 sink 是控制台 sink
    }
  }
}

void LogManager::set_log_save_path(const std::string &path)
{
  get_save_path() = path;
}

void LogManager::flush_all()
{
  auto &loggers = get_logger_map();
  auto &mtx = get_logger_mutex();

  std::vector<std::shared_ptr<spdlog::logger>> all_loggers;
  {
    std::lock_guard<std::mutex> lock(mtx);
    all_loggers.reserve(loggers.size());
    for (const auto &pair : loggers)
    {
      all_loggers.push_back(pair.second);
    }
  }
  for (const auto &logger : all_loggers)
  {
    if (logger) logger->flush();
  }
}