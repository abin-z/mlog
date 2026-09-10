#include "logger/log_manager.h"

#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "logger/daily_folder_rotating_sink.h"

namespace
{
struct logger_entry {
  std::shared_ptr<spdlog::logger> logger;
  spdlog::sink_ptr file_sink;
  spdlog::sink_ptr stdout_sink;
};

// 局部静态资源
std::unordered_map<std::string, logger_entry> &logger_map()
{
  static std::unordered_map<std::string, logger_entry> map;
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
    if (it != loggers.end()) return it->second.logger;
  }

  // 未找到则加锁创建
  std::lock_guard<std::mutex> lock(mtx);
  auto it = loggers.find(module);
  if (it != loggers.end()) return it->second.logger;

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
  std::vector<spdlog::sink_ptr> sinks{file_sink, console_sink};
  auto logger = std::make_shared<spdlog::logger>(module, sinks.begin(), sinks.end());
  spdlog::register_logger(logger);

  logger_entry entry;
  entry.logger = logger;
  entry.file_sink = file_sink;
  entry.stdout_sink = console_sink;
  loggers[module] = std::move(entry);
  return logger;
}

spdlog::sink_ptr LogManager::get_file_sink(const std::string &module)
{
  std::lock_guard<std::mutex> lock(logger_mutex());
  auto &loggers = logger_map();
  auto it = loggers.find(module);
  return it == loggers.end() ? nullptr : it->second.file_sink;
}

spdlog::sink_ptr LogManager::get_stdout_sink(const std::string &module)
{
  std::lock_guard<std::mutex> lock(logger_mutex());
  auto &loggers = logger_map();
  auto it = loggers.find(module);
  return it == loggers.end() ? nullptr : it->second.stdout_sink;
}

bool LogManager::set_file_level(const std::string &module, spdlog::level::level_enum level)
{
  std::lock_guard<std::mutex> lock(logger_mutex());
  auto &loggers = logger_map();
  auto it = loggers.find(module);
  if (it == loggers.end() || !it->second.file_sink) return false;

  it->second.file_sink->set_level(level);
  return true;
}

bool LogManager::set_stdout_level(const std::string &module, spdlog::level::level_enum level)
{
  std::lock_guard<std::mutex> lock(logger_mutex());
  auto &loggers = logger_map();
  auto it = loggers.find(module);
  if (it == loggers.end() || !it->second.stdout_sink) return false;

  it->second.stdout_sink->set_level(level);
  return true;
}

void LogManager::set_options(const log_manager_options &options)
{
  auto &loggers = logger_map();
  std::lock_guard<std::mutex> lock(logger_mutex());

  global_options() = options;
  for (auto &pair : loggers)
  {
    if (pair.second.file_sink)
    {
      pair.second.file_sink->set_level(options.file_level);
    }
    if (pair.second.stdout_sink)
    {
      pair.second.stdout_sink->set_level(options.stdout_level);
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

  logger_entry entry;
  entry.logger = std::move(logger);
  loggers[name] = std::move(entry);
  // 先检查 spdlog 内部是否已有同名 logger
  if (!spdlog::get(name))
  {
    spdlog::register_logger(loggers[name].logger);
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
    if (pair.second.file_sink)
    {
      pair.second.file_sink->set_level(level);
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
    if (pair.second.stdout_sink)
    {
      pair.second.stdout_sink->set_level(level);
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
      all_loggers.push_back(pair.second.logger);
    }
  }
  for (const auto &logger : all_loggers)
  {
    if (logger) logger->flush();
  }
}
