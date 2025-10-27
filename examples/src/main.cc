#include <iostream>

#include "logger/module_logger.h"

int main()
{
  std::cout << "Logger Example" << std::endl;
  ModuleLogger logger("example");
  logger.info("Hello, {}!", "world");
  return 0;
}