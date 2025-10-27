#pragma once
#include <memory>
#include <mutex>
#include <string>

#include "spdlog/spdlog.h"

#if defined(__cplusplus) && __cplusplus >= 201703L && defined(__has_include)
#if __has_include(<filesystem>)
#define GHC_USE_STD_FS
#include <filesystem>
namespace fs = std::filesystem;
#endif
#endif
#ifndef GHC_USE_STD_FS
#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;
#endif

class ModuleLogger
{
 public:
  // 构造函数
  ModuleLogger(const std::string& module_name, const std::string& base_log_path = "logs",
               std::size_t max_file_size = 5 * 1024 * 1024, std::size_t max_files = 3);

  // 日志接口
  template <typename... Args>
  void trace(const char* fmt, const Args&... args)
  {
    log_with_rotate_check([&] { logger_->trace(fmt, args...); });
  }
  template <typename... Args>
  void debug(const char* fmt, const Args&... args)
  {
    log_with_rotate_check([&] { logger_->debug(fmt, args...); });
  }
  template <typename... Args>
  void info(const char* fmt, const Args&... args)
  {
    log_with_rotate_check([&] { logger_->info(fmt, args...); });
  }
  template <typename... Args>
  void warn(const char* fmt, const Args&... args)
  {
    log_with_rotate_check([&] { logger_->warn(fmt, args...); });
  }
  template <typename... Args>
  void error(const char* fmt, const Args&... args)
  {
    log_with_rotate_check([&] { logger_->error(fmt, args...); });
  }
  template <typename... Args>
  void critical(const char* fmt, const Args&... args)
  {
    log_with_rotate_check([&] { logger_->critical(fmt, args...); });
  }

 private:
  std::shared_ptr<spdlog::logger> logger_;
  std::string module_name_;
  std::string base_log_path_;
  std::string current_date_;
  std::size_t max_file_size_;
  std::size_t max_files_;
  std::mutex mtx_;  // 多线程安全

  // 获取今天日期
  std::string get_today_folder() const;

  // 创建当天文件夹
  fs::path make_today_folder(const std::string& base_path) const;

  // 检查日期是否变化，必要时切换 rotating sink
  void check_and_rotate();

  // 日志调用包装，保证线程安全和自动切换
  template <typename Func>
  void log_with_rotate_check(Func f)
  {
    std::lock_guard<std::mutex> lock(mtx_);  // TODO: 优化锁粒度
    check_and_rotate();
    f();
  }
};
