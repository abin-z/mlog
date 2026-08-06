#pragma once
#include <spdlog/details/null_mutex.h>
#include <spdlog/pattern_formatter.h>  // <- for pattern_formatter
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <algorithm>
#include <cctype>
#include <chrono>
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

#include <date/date.h>
#include <date/tz.h>

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
class date_folder_rotating_sink final : public spdlog::sinks::base_sink<Mutex> {
 public:
  /**
   * @brief 构造函数
   * @param base_path 基础日志路径，例如 "./logs"
   * @param log_filename 日志文件名，默认 "log.txt"
   * @param max_size 单个日志文件最大字节数，超过则滚动，默认 100MB
   * @param max_files 最大文件数量，超过则删除最早文件，默认 10
   */
  explicit date_folder_rotating_sink(std::string base_path, std::string log_filename = "log.txt",
                                     size_t max_size = 100 * 1024 * 1024, size_t max_files = 10,
                                     int retention_days = 30) :
    base_path_(std::move(base_path)),
    log_filename_(std::move(log_filename)),
    max_size_(max_size),
    max_files_(max_files),
    retention_days_(date::days{std::max(0, retention_days)})
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
    retention_days_ = date::days{retention_days};
  }
  /** 获取保留的最近天数 */
  int get_retention_days() const noexcept
  {
    return static_cast<int>(retention_days_.count());
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
  std::string base_path_;      ///< 基础日志目录
  std::string log_filename_;   ///< 日志文件名
  size_t max_size_;            ///< 单个文件最大字节数
  size_t max_files_;           ///< 最大文件数量
  date::days retention_days_;  ///< 保留最近多少天的历史日志目录

  /** 内部实际使用的 rotating sink 类型（根据 Mutex 选择 mt 或 st） */
  using internal_sink_t =
    typename std::conditional<std::is_same<Mutex, std::mutex>::value, spdlog::sinks::rotating_file_sink_mt,
                              spdlog::sinks::rotating_file_sink_st>::type;

  fs::path current_log_path_;
  std::unique_ptr<internal_sink_t> internal_sink_;
  std::chrono::system_clock::time_point next_roll_time_;
  /** 获取时间对应的日期字符串，例如 "2025-10-28" */
  template <typename Duration>
  static std::string date_str(const date::local_time<Duration> &local_time)
  {
    return date::format("%F", date::floor<date::days>(local_time));
  }

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
  /** 删除超过保留天数的旧日期目录 */
  void clean_old_directories(const date::year_month_day &current_local_day)
  {
    if (!fs::exists(base_path_))
    {
      return;
    }

    // retention_days_:
    //   0  -> 仅保留当天日志
    //   1  -> 保留最近1天历史 + 当天日志
    //   30 -> 保留最近30天历史 + 当天日志
    const std::string cutoff = date::format("%F", date::sys_days{current_local_day} - retention_days_);

    for (fs::directory_iterator it(base_path_), end; it != end; ++it)
    {
      if (!it->is_directory())
      {
        continue;
      }

      const std::string dir_name = it->path().filename().string();

      // 仅处理 YYYY-MM-DD 格式目录
      if (!is_date_dir_name(dir_name))
      {
        continue;
      }

      // YYYY-MM-DD 字典序即日期顺序
      if (dir_name < cutoff)
      {
        std::error_code ec;
        fs::remove_all(it->path(), ec);

        // TODO: 删除失败可记录日志
        // if (ec) { ... }
      }
    }
  }

  /** 滚动到今天，创建对应日期文件夹和日志文件 */
  void roll_to_today()
  {
    auto now = std::chrono::system_clock::now();

    // 获取当前时区，并转换成本地时间
    const auto *zone = date::current_zone();
    auto z = date::make_zoned(zone, now);
    auto local_time = z.get_local_time();

    // 生成日期目录
    auto folder = fs::path(base_path_) / date_str(local_time);
    fs::create_directories(folder);

    auto full_path = folder / log_filename_;
    current_log_path_ = full_path;

    // 新建 rotating sink（选择 mt 或 st）
    auto new_sink = spdlog::details::make_unique<internal_sink_t>(full_path.string(), max_size_, max_files_, false);

    // 继承已有 formatter（如果有）
    if (this->formatter_) new_sink->set_formatter(this->formatter_->clone());

    if (internal_sink_) internal_sink_->flush();
    internal_sink_ = std::move(new_sink);

    // 清理早于保留窗口的旧日期目录
    const auto current_local_day = date::year_month_day{date::floor<date::days>(local_time)};
    clean_old_directories(current_local_day);

    // 计算下次切换时间：本地时区次日 00:00
    const auto local_today = date::floor<date::days>(local_time);
    const auto local_tomorrow = local_today + date::days{1};
    // 本地时间转换回 system_clock 时间
    date::zoned_time<std::chrono::system_clock::duration> zt{zone, local_tomorrow};

    next_roll_time_ = zt.get_sys_time();
  }
};

// 便捷别名
using date_folder_rotating_sink_mt = date_folder_rotating_sink<std::mutex>;
using date_folder_rotating_sink_st = date_folder_rotating_sink<spdlog::details::null_mutex>;
