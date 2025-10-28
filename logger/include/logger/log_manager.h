#pragma once

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

class LogManager
{
 public:
  LogManager() = default;  // 禁止实例化
  LogManager(const LogManager&) = delete;
  LogManager& operator=(const LogManager&) = delete;
  /// 获取指定模块的 logger（若不存在则创建）
  static std::shared_ptr<spdlog::logger> getLogger(const std::string& module);

  /// 设置全局日志级别（会影响所有 logger）
  static void setGlobalLevel(spdlog::level::level_enum level);
};
