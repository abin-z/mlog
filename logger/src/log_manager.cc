#include "logger/log_manager.h"

#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "logger/daily_folder_rotating_sink.h"

namespace
{
// 局部静态资源
std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> &logger_map()
{
  static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> map;
  return map;
}

std::mutex &logger_mutex()
{
  static std::mutex mtx;
  return mtx;
}

log_manager_options &global_options()
{
  static log_manager_options options;
  return options;
}

}  // namespace

std::shared_ptr<spdlog::logger> LogManager::get_logger(const std::string &module)
{
  auto &loggers = logger_map();
  auto &mtx = logger_mutex();

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

  const log_manager_options manager_options = global_options();
  daily_folder_rotating_sink_options sink_options;
  sink_options.base_path = manager_options.save_path;
  sink_options.log_filename = module + ".log";
  sink_options.max_size = manager_options.max_size;
  sink_options.max_files = manager_options.max_files;
  sink_options.retention_days = manager_options.retention_days;

  // === 文件 Sink（每天一个文件夹）===
  auto file_sink = std::make_shared<daily_folder_rotating_sink_mt>(std::move(sink_options));
  file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
  file_sink->set_level(manager_options.file_level);

  // === 控制台 Sink（带颜色）===
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
  console_sink->set_level(manager_options.stdout_level);

  // === 创建 logger（同时绑定多个 sink）===
  std::vector<spdlog::sink_ptr> sinks{file_sink, console_sink};  // 顺序不能变, 文件 sink 在前
  auto logger = std::make_shared<spdlog::logger>(module, sinks.begin(), sinks.end());
  spdlog::register_logger(logger);

  loggers[module] = logger;
  return logger;
}

void LogManager::set_options(const log_manager_options &options)
{
  auto &loggers = logger_map();
  std::lock_guard<std::mutex> lock(logger_mutex());

  global_options() = options;
  for (auto &pair : loggers)
  {
    if (!pair.second->sinks().empty())
    {
      pair.second->sinks()[0]->set_level(options.file_level);
    }
    if (pair.second->sinks().size() > 1)
    {
      pair.second->sinks()[1]->set_level(options.stdout_level);
    }
  }
}

log_manager_options LogManager::get_options()
{
  std::lock_guard<std::mutex> lock(logger_mutex());
  return global_options();
}

bool LogManager::add_logger(std::shared_ptr<spdlog::logger> logger)
{
  if (!logger) return false;

  auto &loggers = logger_map();
  auto &mtx = logger_mutex();
  auto name = logger->name();

  std::lock_guard<std::mutex> lock(mtx);
  auto it = loggers.find(name);
  if (it != loggers.end()) return false;  // 已存在同名 logger

  loggers[name] = std::move(logger);
  // 先检查 spdlog 内部是否已有同名 logger
  if (!spdlog::get(name))
  {
    spdlog::register_logger(loggers[name]);
  }
  return true;
}

void LogManager::set_file_global_level(spdlog::level::level_enum level)
{
  auto &loggers = logger_map();
  auto &mtx = logger_mutex();
  std::lock_guard<std::mutex> lock(mtx);

  global_options().file_level = level;
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
  auto &loggers = logger_map();
  auto &mtx = logger_mutex();
  std::lock_guard<std::mutex> lock(mtx);

  global_options().stdout_level = level;
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
  std::lock_guard<std::mutex> lock(logger_mutex());
  global_options().save_path = path;
}

void LogManager::set_log_max_size(std::size_t size)
{
  std::lock_guard<std::mutex> lock(logger_mutex());
  if (size > 0) global_options().max_size = size;
}

void LogManager::set_log_max_files(std::size_t count)
{
  std::lock_guard<std::mutex> lock(logger_mutex());
  if (count > 0) global_options().max_files = count;
}

void LogManager::set_log_rotation(std::size_t log_max_size, std::size_t log_max_files)
{
  set_log_max_size(log_max_size);
  set_log_max_files(log_max_files);
}

void LogManager::set_log_retention_days(std::size_t days)
{
  std::lock_guard<std::mutex> lock(logger_mutex());
  global_options().retention_days = days;
}

void LogManager::flush_all()
{
  auto &loggers = logger_map();
  auto &mtx = logger_mutex();

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
