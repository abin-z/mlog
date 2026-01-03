#pragma once

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

/**
 * @brief 全局日志管理器
 *
 * LogManager 提供统一接口获取不同模块的 logger, 并支持全局日志级别控制.
 *
 * 设计特点:
 * - 单例风格: 禁止实例化和拷贝, 只提供静态接口
 * - 每个模块可获取独立的 logger
 * - 支持文件日志和控制台日志（可在内部配置不同 sink）
 * - 支持设置全局日志级别, 影响所有已创建的 logger
 */
class LogManager
{
 public:
  LogManager() = delete;                               ///< 禁止默认构造
  LogManager(const LogManager &) = delete;             ///< 禁止拷贝构造
  LogManager &operator=(const LogManager &) = delete;  ///< 禁止赋值
  LogManager(LogManager &&) = delete;                  ///< 禁止移动构造
  LogManager &operator=(LogManager &&) = delete;       ///< 禁止移动赋值

  /**
   * @brief 设置日志文件的保存目录
   *
   * 用于指定所有文件日志（file sink）的统一输出目录.
   *
   * 行为说明:
   * - 该路径会影响后续创建的所有 logger 的文件日志位置
   * - 若在 logger 创建之后调用, 仅影响之后新创建的 logger
   * - 不负责创建目录, 目录不存在时由 spdlog 自身行为决定（可能失败）
   *
   * @param path 日志文件保存目录（绝对或相对路径）
   */
  static void set_log_save_path(const std::string &path);

  /**
   * @brief 设置日志文件的最大大小（字节数）
   *
   * 用于指定所有文件日志（file sink）的单个日志文件最大大小.
   *
   * 行为说明:
   * - 该大小会影响后续创建的所有 logger 的文件日志行为
   * - 若在 logger 创建之后调用, 仅影响之后新创建的 logger
   * - 超过该大小时会进行日志滚动（rotating）
   *
   * @param size 单个日志文件最大大小（字节数）
   */
  static void set_log_max_size(std::size_t size);

  /**
   * @brief 设置日志文件的最大数量
   *
   * 用于指定所有文件日志（file sink）的最大日志文件数量.
   *
   * 行为说明:
   * - 该数量会影响后续创建的所有 logger 的文件日志行为
   * - 若在 logger 创建之后调用, 仅影响之后新创建的 logger
   * - 超过该数量时会删除最早的日志文件
   *
   * @param count 最大日志文件数量
   */
  static void set_log_max_files(std::size_t count);

  /**
   * @brief 设置日志文件的滚动策略
   *
   * 用于指定所有文件日志（file sink）的滚动大小和文件数量.
   *
   * 行为说明:
   * - 该策略会影响后续创建的所有 logger 的文件日志行为
   * - 若在 logger 创建之后调用, 仅影响之后新创建的 logger
   * - 超过该大小时会进行日志滚动（rotating）
   * - 超过该数量时会删除最早的日志文件
   *
   * @param max_size 单个日志文件最大大小（字节数）
   * @param max_files 最大日志文件数量
   */
  static void set_log_rotation(std::size_t log_max_size, std::size_t log_max_files);

  /**
   * @brief 获取指定模块的 logger
   *
   * 如果该模块的 logger 尚未创建, 则会自动创建一个新的 logger,
   * 并注册到 spdlog 内部.
   *
   * @param module 模块名, 例如 "web", "service", "db"
   * @return std::shared_ptr<spdlog::logger> 模块 logger 指针
   */
  static std::shared_ptr<spdlog::logger> get_logger(const std::string &module);

  /**
   * @brief 将已有 logger 添加到 LogManager 管理（并注册到 spdlog 全局注册表）
   * @param logger 需要添加的 std::shared_ptr<spdlog::logger> 对象
   * @return 如果 logger 为空或已存在同名 logger 返回 false, 否则返回 true
   * @note 添加后 LogManager 会接管管理该 logger 的生命周期
   */
  static bool add_logger(std::shared_ptr<spdlog::logger> logger);

  /**
   * @brief 设置文件日志全局级别
   *
   * 会遍历所有已创建的 logger 并更新其文件 sink 的日志级别,
   * 同时新创建的 logger 也会继承该级别.
   *
   * @param level spdlog::level::level_enum 日志级别, 例如 spdlog::level::info
   */
  static void set_file_global_level(spdlog::level::level_enum level);

  /**
   * @brief 设置控制台日志全局级别
   *
   * 会遍历所有已创建的 logger 并更新其控制台 sink 的日志级别,
   * 同时新创建的 logger 也会继承该级别.
   *
   * @param level spdlog::level::level_enum 日志级别, 例如 spdlog::level::warn
   */
  static void set_stdout_global_level(spdlog::level::level_enum level);

  /**
   * @brief 刷新所有 logger 的日志缓冲区
   *
   * 会调用 spdlog::logger::flush() 来确保所有日志都写入到对应的输出目标.
   */
  static void flush_all();
};
