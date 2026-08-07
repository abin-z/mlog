#pragma once
#include <spdlog/details/null_mutex.h>
#include <spdlog/pattern_formatter.h>  // <- for pattern_formatter
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <memory>
#include <mutex>
#include <type_traits>

#if defined(__cpp_lib_filesystem) && __cpp_lib_filesystem >= 201703L
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<filesystem>) && defined(__cplusplus) && __cplusplus >= 201703L
#include <filesystem>
namespace fs = std::filesystem;
#else
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
 * auto sink = std::make_shared<daily_folder_rotating_sink_mt>("./logs", "app.log", 100*1024*1024, 10);
 * sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
 * logger->sinks().push_back(sink);
 * @endcode
 *
 * @tparam Mutex 日志线程安全策略，std::mutex 表示多线程安全，spdlog::details::null_mutex 表示单线程
 */
template <typename Mutex>
class daily_folder_rotating_sink final : public spdlog::sinks::base_sink<Mutex> {
 public:
  /**
   * @brief 构造函数
   * @param base_path 基础日志路径，例如 "./logs"
   * @param log_filename 日志文件名，默认 "log.txt"
   * @param max_size 单个日志文件最大字节数，超过则滚动，默认 100MB
   * @param max_files 最大文件数量，超过则删除最早文件，默认 10
   */
  explicit daily_folder_rotating_sink(std::string base_path, std::string log_filename = "log.txt",
                                     size_t max_size = 100 * 1024 * 1024, size_t max_files = 10,
                                     int retention_days = 30) :
    base_path_(std::move(base_path)),
    log_filename_(std::move(log_filename)),
    max_size_(max_size),
    max_files_(max_files),
    retention_days_(std::max(0, retention_days))
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
  std::size_t get_max_size() const noexcept
  {
    if (internal_sink_) return internal_sink_->get_max_size();
    return max_size_;
  }
  /** 获取最大日志文件数量 */
  std::size_t get_max_files() const noexcept
  {
    if (internal_sink_) return internal_sink_->get_max_files();
    return max_files_;
  }
  /** 设置保留的最近天数 */
  void set_retention_days(int retention_days)
  {
    if (retention_days < 0) retention_days = 0;
    retention_days_ = retention_days;
  }
  /** 获取保留的最近天数 */
  int get_retention_days() const noexcept
  {
    return retention_days_;
  }
  /** 获取当前正在写入的日志文件路径 */
  fs::path current_log_path() const noexcept
  {
    return current_log_path_;
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
  int retention_days_;  // 保留最近多少天的日志目录, 0表示只保留今天, 1表示保留昨天+今天, 30表示保留最近30天+今天

  /** 内部实际使用的 rotating sink 类型（根据 Mutex 选择 mt 或 st） */
  using internal_sink_t =
    typename std::conditional<std::is_same<Mutex, std::mutex>::value, spdlog::sinks::rotating_file_sink_mt,
                              spdlog::sinks::rotating_file_sink_st>::type;

  fs::path current_log_path_;
  std::unique_ptr<internal_sink_t> internal_sink_;
  std::chrono::system_clock::time_point next_roll_time_;

  /** 判断是否为 YYYY-MM-DD 格式的日期目录 */
  static bool is_date_dir_name(const std::string &name) noexcept
  {
    return name.size() == 10 && (std::isdigit(static_cast<unsigned char>(name[0])) != 0) &&
           (std::isdigit(static_cast<unsigned char>(name[1])) != 0) &&
           (std::isdigit(static_cast<unsigned char>(name[2])) != 0) &&
           (std::isdigit(static_cast<unsigned char>(name[3])) != 0) && name[4] == '-' &&
           (std::isdigit(static_cast<unsigned char>(name[5])) != 0) &&
           (std::isdigit(static_cast<unsigned char>(name[6])) != 0) && name[7] == '-' &&
           (std::isdigit(static_cast<unsigned char>(name[8])) != 0) &&
           (std::isdigit(static_cast<unsigned char>(name[9])) != 0);
  }

  /**
   * @brief 获取系统本地时间
   */
  static std::tm local_tm()
  {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::tm tm{};

#if defined(_WIN32)
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif

    return tm;
  }

  /**
   * 删除过期日期目录
   *
   * 规则：
   *
   * retention_days_ = 0
   *   保留今天
   *
   * retention_days_ = 1
   *   保留昨天 + 今天
   *
   * retention_days_ = 30
   *   保留最近30天 + 今天
   */
  void clean_old_directories(const std::tm &current_tm)
  {
    std::error_code eec;
    if (!fs::exists(base_path_, eec) || eec)
    {
      report_error("Base path does not exist or error: " + eec.message() + ", skipping cleanup");
      return;
    }

    // 计算截止时间
    std::tm cutoff_tm = current_tm;
    cutoff_tm.tm_hour = 0;
    cutoff_tm.tm_min = 0;
    cutoff_tm.tm_sec = 0;
    std::time_t cutoff_time = std::mktime(&cutoff_tm);

    // 往前推保留天数
    cutoff_time -= static_cast<std::time_t>(retention_days_) * 24 * 60 * 60;
    std::tm limit_tm{};

#if defined(_WIN32)
    localtime_s(&limit_tm, &cutoff_time);
#else
    localtime_r(&cutoff_time, &limit_tm);
#endif

    char cutoff_date[16];

    const int len = std::snprintf(cutoff_date, sizeof(cutoff_date), "%04d-%02d-%02d", limit_tm.tm_year + 1900,
                                  limit_tm.tm_mon + 1, limit_tm.tm_mday);

    if (len < 0 || static_cast<size_t>(len) >= sizeof(cutoff_date))
    {
      throw spdlog::spdlog_ex("Failed to format cutoff date");
    }
    std::error_code diec;
    for (fs::directory_iterator it(base_path_, diec), end; it != end && !diec; ++it)
    {
      if (!it->is_directory())
      {
        continue;
      }
      const std::string dir_name = it->path().filename().string();

      // 非日期目录忽略
      if (!is_date_dir_name(dir_name))
      {
        continue;
      }

      // YYYY-MM-DD 可以直接字符串比较, 如果目录名小于 cutoff_date，则删除
      if (dir_name < cutoff_date)
      {
        std::error_code ec;
        fs::remove_all(it->path(), ec);
        if (ec)
        {
          report_error("Failed to remove old log directory: " + it->path().string() + ", error: " + ec.message());
        }
      }
    }
    if (diec)
    {
      report_error("Failed to iterate log directory");
    }
  }

  static void ensure_directory(const fs::path &path)
  {
    std::error_code ec;
    fs::create_directories(path, ec);
    if (ec)
    {
      throw spdlog::spdlog_ex("Failed to create directory: " + path.string() + ", error: " + ec.message());
    }
  }

  static void report_error(const std::string &msg) noexcept
  {
    (void)std::fprintf(stderr, "[daily_folder_rotating_sink] %s\n", msg.c_str());
  }

  /** 滚动到今天，创建对应日期文件夹和日志文件 */
  void roll_to_today()
  {
    const std::tm tm = local_tm();

    // 创建日期目录

    char date_buffer[16];

    const int len =
      std::snprintf(date_buffer, sizeof(date_buffer), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

    if (len < 0 || static_cast<size_t>(len) >= sizeof(date_buffer))
    {
      throw spdlog::spdlog_ex("Invalid date format");
    }
    const fs::path folder = fs::path(base_path_) / date_buffer;

    ensure_directory(folder);

    const fs::path log_path = folder / log_filename_;

    current_log_path_ = log_path;

    // 创建新的 rotating sink
    auto new_sink = spdlog::details::make_unique<internal_sink_t>(log_path.string(), max_size_, max_files_, false);

    // 保留 formatter
    if (this->formatter_) new_sink->set_formatter(this->formatter_->clone());

    // 切换 sink
    if (internal_sink_) internal_sink_->flush();

    internal_sink_ = std::move(new_sink);

    // 清理旧目录
    clean_old_directories(tm);

    // 计算下一次滚动时间 明天 00:00
    std::tm next_tm = tm;
    next_tm.tm_hour = 0;
    next_tm.tm_min = 0;
    next_tm.tm_sec = 0;
    next_tm.tm_mday += 1;
    next_tm.tm_isdst = -1;

    const std::time_t next_time = std::mktime(&next_tm);
    next_roll_time_ = std::chrono::system_clock::from_time_t(next_time);
  }
};

// 便捷别名
using daily_folder_rotating_sink_mt = daily_folder_rotating_sink<std::mutex>;
using daily_folder_rotating_sink_st = daily_folder_rotating_sink<spdlog::details::null_mutex>;
