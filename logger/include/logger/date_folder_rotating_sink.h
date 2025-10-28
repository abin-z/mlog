#pragma once
#include <spdlog/details/null_mutex.h>
#include <spdlog/pattern_formatter.h>  // <- for pattern_formatter
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <chrono>
#include <ctime>
#include <memory>
#include <mutex>
#include <type_traits>

#if defined(__cplusplus) && __cplusplus >= 201703L && defined(__has_include)
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#endif
#endif
#ifndef fs
#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;
#endif

template <typename Mutex>
class date_folder_rotating_sink final : public spdlog::sinks::base_sink<Mutex>
{
 public:
  date_folder_rotating_sink(std::string base_path, std::string log_filename = "log.txt",
                            size_t max_size = 100 * 1024 * 1024, size_t max_files = 10) :
    base_path_(std::move(base_path)), log_filename_(std::move(log_filename)), max_size_(max_size), max_files_(max_files)
  {
    roll_to_today();
  }

 protected:
  // 必须实现：写日志
  void sink_it_(const spdlog::details::log_msg &msg) override
  {
    auto now = std::chrono::system_clock::now();
    if (now >= next_roll_time_)
    {
      roll_to_today();
    }
    if (internal_sink_) internal_sink_->log(msg);
  }

  // 必须实现：flush
  void flush_() override
  {
    if (internal_sink_) internal_sink_->flush();
  }

  // 注意：重写带下划线的方法！base_sink 会在 set_pattern() 调用时加锁并转而调用此方法。
  void set_pattern_(const std::string &pattern) override
  {
    // base_sink::set_pattern_ 的默认实现会创建一个 pattern_formatter，
    // 我们也要做同样的事，并同步到内部 sink
    this->formatter_ = std::make_unique<spdlog::pattern_formatter>(pattern);
    if (internal_sink_) internal_sink_->set_formatter(this->formatter_->clone());
  }

  // 同理重写 set_formatter_，base_sink::set_formatter() 会调用此方法（并加锁）
  void set_formatter_(std::unique_ptr<spdlog::formatter> sink_formatter) override
  {
    // 保存 formatter（拥有所有权）
    this->formatter_ = std::move(sink_formatter);
    if (internal_sink_) internal_sink_->set_formatter(this->formatter_->clone());
  }

 private:
  std::string base_path_;
  std::string log_filename_;
  size_t max_size_;
  size_t max_files_;

  using internal_sink_t =
    typename std::conditional<std::is_same<Mutex, std::mutex>::value, spdlog::sinks::rotating_file_sink_mt,
                              spdlog::sinks::rotating_file_sink_st>::type;

  std::unique_ptr<internal_sink_t> internal_sink_;
  std::chrono::system_clock::time_point next_roll_time_;

  static std::string date_str(const std::chrono::system_clock::time_point &tp)
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
    return buf;
  }

  void roll_to_today()
  {
    auto now = std::chrono::system_clock::now();

    // 生成日期目录
    auto folder = fs::path(base_path_) / date_str(now);
    fs::create_directories(folder);

    auto full_path = (folder / log_filename_).string();

    // 新建 rotating sink（选择 mt 或 st）
    auto new_sink = std::make_unique<internal_sink_t>(full_path, max_size_, max_files_, false);

    // 继承已有 formatter（如果有）
    if (this->formatter_) new_sink->set_formatter(this->formatter_->clone());

    internal_sink_ = std::move(new_sink);

    // 计算下次切换时间（次日 00:00:00）
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_mday += 1;
    std::time_t next_day = std::mktime(&tm);
    next_roll_time_ = std::chrono::system_clock::from_time_t(next_day);
  }
};

// 便捷别名
using date_folder_rotating_sink_mt = date_folder_rotating_sink<std::mutex>;
using date_folder_rotating_sink_st = date_folder_rotating_sink<spdlog::details::null_mutex>;
