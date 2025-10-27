#include "logger/module_logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

ModuleLogger::ModuleLogger(const std::string& module_name, const std::string& base_log_path, std::size_t max_file_size,
                           std::size_t max_files) :
  module_name_(module_name), base_log_path_(base_log_path), max_file_size_(max_file_size), max_files_(max_files)
{
  current_date_ = get_today_folder();

  std::vector<spdlog::sink_ptr> sinks;

  // 控制台 sink
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
  sinks.push_back(console_sink);

  // 文件 sink
  fs::path folder = make_today_folder(base_log_path_);
  fs::path file_path = folder / (module_name_ + ".log");

  auto file_sink =
    std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_path.string(), max_file_size_, max_files_);
  file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
  sinks.push_back(file_sink);

  logger_ = std::make_shared<spdlog::logger>(module_name_, sinks.begin(), sinks.end());
  logger_->set_level(spdlog::level::debug);
  logger_->flush_on(spdlog::level::info);

  spdlog::register_logger(logger_);
}

std::string ModuleLogger::get_today_folder() const
{
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d");
  return oss.str();
}

fs::path ModuleLogger::make_today_folder(const std::string& base_path) const
{
  fs::path folder = fs::path(base_path) / get_today_folder();
  if (!fs::exists(folder))
  {
    fs::create_directories(folder);
  }
  return folder;
}

void ModuleLogger::check_and_rotate()
{
  std::string today = get_today_folder();
  if (today != current_date_)
  {
    current_date_ = today;

    // 创建新文件夹
    fs::path folder = make_today_folder(base_log_path_);
    fs::path file_path = folder / (module_name_ + ".log");

    // 新的 rotating sink
    auto new_sink =
      std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_path.string(), max_file_size_, max_files_);
    new_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");

    // 替换 logger 的文件 sink（保留控制台 sink）
    std::vector<spdlog::sink_ptr> new_sinks;
    for (auto& s : logger_->sinks())
    {
      if (std::dynamic_pointer_cast<spdlog::sinks::rotating_file_sink_mt>(s))
        new_sinks.push_back(new_sink);
      else
        new_sinks.push_back(s);
    }
    logger_->sinks() = new_sinks;
  }
}
