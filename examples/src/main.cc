#include <iostream>

#include "logger/module_logger.h"
#include "logger/date_folder_rotating_sink.h"

int main()
{
  std::cout << "Logger Example" << std::endl;
  ModuleLogger logger("example");
  auto sink = std::make_shared<date_folder_rotating_sink_mt>("./logs", 5*1024*1024, 3);
  logger.info("Hello, {}!", "world");
  return 0;
}