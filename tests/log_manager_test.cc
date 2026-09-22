#include "logger/log_manager.h"

#include <spdlog/sinks/null_sink.h>

#include <cassert>
#include <memory>

int main()
{
  log_manager_options options;
  options.save_path = "./test_logs";
  LogManager::set_options(options);

  const auto managed_logger = LogManager::get_logger("managed");
  const auto file_sink = LogManager::get_file_sink("managed");
  const auto stdout_sink = LogManager::get_stdout_sink("managed");

  assert(managed_logger);
  assert(file_sink);
  assert(stdout_sink);
  assert(LogManager::set_file_level("managed", spdlog::level::debug));
  assert(LogManager::set_stdout_level("managed", spdlog::level::err));
  assert(file_sink->level() == spdlog::level::debug);
  assert(stdout_sink->level() == spdlog::level::err);

  assert(!LogManager::get_file_sink("missing"));
  assert(!LogManager::get_stdout_sink("missing"));
  assert(!LogManager::set_file_level("missing", spdlog::level::info));
  assert(!LogManager::set_stdout_level("missing", spdlog::level::info));

  const auto external_without_sinks = std::make_shared<spdlog::logger>("external_without_sinks");
  assert(LogManager::add_logger(external_without_sinks));
  assert(LogManager::get_logger("external_without_sinks") == external_without_sinks);
  assert(!LogManager::get_file_sink("external_without_sinks"));
  assert(!LogManager::get_stdout_sink("external_without_sinks"));
  assert(!LogManager::set_file_level("external_without_sinks", spdlog::level::debug));
  assert(!LogManager::set_stdout_level("external_without_sinks", spdlog::level::debug));

  const auto external_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
  external_sink->set_level(spdlog::level::trace);
  const auto external_with_sink = std::make_shared<spdlog::logger>("external_with_sink", external_sink);
  assert(LogManager::add_logger(external_with_sink));

  LogManager::set_file_global_level(spdlog::level::critical);
  LogManager::set_stdout_global_level(spdlog::level::off);

  assert(file_sink->level() == spdlog::level::critical);
  assert(stdout_sink->level() == spdlog::level::off);
  assert(external_sink->level() == spdlog::level::trace);
  assert(!LogManager::get_file_sink("external_with_sink"));
  assert(!LogManager::get_stdout_sink("external_with_sink"));
  LogManager::flush_all();
}
