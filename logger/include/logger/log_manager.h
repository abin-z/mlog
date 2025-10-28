#pragma once

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

/**
 * @brief 全局日志管理器
 *
 * LogManager 提供统一接口获取不同模块的 logger，并支持全局日志级别控制。
 *
 * 设计特点：
 * - 单例风格：禁止实例化和拷贝，只提供静态接口
 * - 每个模块可获取独立的 logger
 * - 支持文件日志和控制台日志（可在内部配置不同 sink）
 * - 支持设置全局日志级别，影响所有已创建的 logger
 */
class LogManager
{
 public:
  LogManager() = delete;                              ///< 禁止默认构造
  LogManager(const LogManager&) = delete;             ///< 禁止拷贝构造
  LogManager& operator=(const LogManager&) = delete;  ///< 禁止赋值

  /**
   * @brief 获取指定模块的 logger
   *
   * 如果该模块的 logger 尚未创建，则会自动创建一个新的 logger，
   * 并注册到 spdlog 内部。
   *
   * @param module 模块名，例如 "web"、"service"、"db"
   * @return std::shared_ptr<spdlog::logger> 模块 logger 指针
   */
  static std::shared_ptr<spdlog::logger> getLogger(const std::string& module);

  /**
   * @brief 设置文件日志全局级别
   *
   * 会遍历所有已创建的 logger 并更新其文件 sink 的日志级别，
   * 同时新创建的 logger 也会继承该级别。
   *
   * @param level spdlog::level::level_enum 日志级别，例如 spdlog::level::info
   */
  static void setFileGlobalLevel(spdlog::level::level_enum level);

  /**
   * @brief 设置控制台日志全局级别
   *
   * 会遍历所有已创建的 logger 并更新其控制台 sink 的日志级别，
   * 同时新创建的 logger 也会继承该级别。
   *
   * @param level spdlog::level::level_enum 日志级别，例如 spdlog::level::warn
   */
  static void setStdoutGlobalLevel(spdlog::level::level_enum level);
};
