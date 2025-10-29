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

/**
 * @brief 日期文件夹滚动日志 Sink
 *
 * 这个 Sink 会根据当前日期在 base_path 下创建每天独立的文件夹，然后在其中生成滚动日志文件。
 * 内部实际使用 spdlog 的 rotating_file_sink（多线程或单线程版本）来支持日志文件大小限制和文件数量限制。
 *
 * 特性：
 * - 每天自动创建一个新文件夹存放日志
 * - 支持按大小滚动日志文件
 * - 支持最大文件数量限制
 * - 支持设置日志格式（pattern）
 * - 支持多线程或单线程模式
 *
 * 使用示例：
 * @code
 * auto sink = std::make_shared<date_folder_rotating_sink_mt>("./logs", "app.log", 100*1024*1024, 10);
 * sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
 * logger->sinks().push_back(sink);
 * @endcode
 *
 * @tparam Mutex 日志线程安全策略，std::mutex 表示多线程安全，spdlog::details::null_mutex 表示单线程
 */
template <typename Mutex>
class date_folder_rotating_sink final : public spdlog::sinks::base_sink<Mutex>
{
 public:
  /**
   * @brief 构造函数
   * @param base_path 基础日志路径，例如 "./logs"
   * @param log_filename 日志文件名，默认 "log.txt"
   * @param max_size 单个日志文件最大字节数，超过则滚动，默认 100MB
   * @param max_files 最大文件数量，超过则删除最早文件，默认 10
   */
  date_folder_rotating_sink(std::string base_path, std::string log_filename = "log.txt",
                            size_t max_size = 100 * 1024 * 1024, size_t max_files = 10) :
    base_path_(std::move(base_path)), log_filename_(std::move(log_filename)), max_size_(max_size), max_files_(max_files)
  {
    roll_to_today();
  }
  /** 设置单个日志文件最大大小 */
  void set_max_size(size_t max_size)
  {
    max_size_ = max_size;
    if (internal_sink_) internal_sink_->set_max_size(max_size_);
  }
  /** 设置最大日志文件数量 */
  void set_max_files(size_t max_files)
  {
    max_files_ = max_files;
    if (internal_sink_) internal_sink_->set_max_files(max_files_);
  }
  /** 获取单个日志文件最大大小 */
  std::size_t get_max_size()
  {
    if (internal_sink_) return internal_sink_->get_max_size();
    return max_size_;
  }
  /** 获取最大日志文件数量 */
  std::size_t get_max_files()
  {
    if (internal_sink_) return internal_sink_->get_max_files();
    return max_files_;
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

  // 重写带下划线的方法！base_sink 会在 set_pattern() 调用时加锁并转而调用此方法。
  void set_pattern_(const std::string &pattern) override
  {
    // base_sink::set_pattern_ 的默认实现会创建一个 pattern_formatter，
    // 我们也要做同样的事，并同步到内部 sink
    this->formatter_ = spdlog::details::make_unique<spdlog::pattern_formatter>(pattern);
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
  std::string base_path_;     ///< 基础日志目录
  std::string log_filename_;  ///< 日志文件名
  size_t max_size_;           ///< 单个文件最大字节数
  size_t max_files_;          ///< 最大文件数量

  /** 内部实际使用的 rotating sink 类型（根据 Mutex 选择 mt 或 st） */
  using internal_sink_t =
    typename std::conditional<std::is_same<Mutex, std::mutex>::value, spdlog::sinks::rotating_file_sink_mt,
                              spdlog::sinks::rotating_file_sink_st>::type;

  std::unique_ptr<internal_sink_t> internal_sink_;
  std::chrono::system_clock::time_point next_roll_time_;
  /** 获取时间对应的日期字符串，例如 "2025-10-28" */
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
  /** 滚动到今天，创建对应日期文件夹和日志文件 */
  void roll_to_today()
  {
    auto now = std::chrono::system_clock::now();

    // 生成日期目录
    auto folder = fs::path(base_path_) / date_str(now);
    fs::create_directories(folder);

    auto full_path = (folder / log_filename_).string();

    // 新建 rotating sink（选择 mt 或 st）
    auto new_sink = spdlog::details::make_unique<internal_sink_t>(full_path, max_size_, max_files_, false);

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
