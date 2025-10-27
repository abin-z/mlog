#pragma once
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <chrono>
#include <ctime>
#include <memory>
#include <mutex>
#include <type_traits>

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

template <typename Mutex>
class date_folder_rotating_sink final : public spdlog::sinks::base_sink<Mutex>
{
 public:
  date_folder_rotating_sink(const std::string& base_path, const std::string& log_file_name = "log.txt",
                            size_t max_size = 100 * 1024 * 1024, size_t max_files = 9) :
    base_path_(base_path), log_file_name_(log_file_name), max_size_(max_size), max_files_(max_files)
  {
    roll_if_needed(std::chrono::system_clock::now());
  }

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override
  {
    auto now = std::chrono::system_clock::now();
    if (now >= next_roll_time_)
    {
      roll_if_needed(now);
    }
    rotating_sink_->log(msg);
  }

  void flush_() override
  {
    rotating_sink_->flush();
  }

 private:
  std::string base_path_;
  std::string log_file_name_;
  size_t max_size_;
  size_t max_files_;

  using internal_sink_t =
    typename std::conditional<std::is_same<Mutex, std::mutex>::value, spdlog::sinks::rotating_file_sink_mt,
                              spdlog::sinks::rotating_file_sink_st>::type;

  std::unique_ptr<internal_sink_t> rotating_sink_;
  std::chrono::system_clock::time_point next_roll_time_;

  // 获取日期字符串 YYYY-MM-DD
  static std::string date_str(const std::chrono::system_clock::time_point& tp)
  {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return std::string(buf);
  }

  void roll_if_needed(const std::chrono::system_clock::time_point& now)
  {
    // 创建日期文件夹
    std::string date = date_str(now);
    fs::path folder = fs::path(base_path_) / date;
    fs::create_directories(folder);

    // 使用自定义文件名
    std::string log_file = (folder / log_file_name_).string();
    rotating_sink_.reset(new internal_sink_t(log_file, max_size_, max_files_, false)); // TODO: 日志格式丢失 

    // 计算当天最后时刻的 time_point
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    tm.tm_hour = 23;
    tm.tm_min = 59;
    tm.tm_sec = 59;

    std::time_t last_sec = std::mktime(&tm);
    next_roll_time_ = std::chrono::system_clock::from_time_t(last_sec) + std::chrono::milliseconds(999);

    // 继承格式化器
    if (this->formatter_)
    {
      rotating_sink_->set_formatter(this->formatter_->clone());
    }
  }
};

// 方便使用的别名
using date_folder_rotating_sink_mt = date_folder_rotating_sink<std::mutex>;
using date_folder_rotating_sink_st = date_folder_rotating_sink<spdlog::details::null_mutex>;
