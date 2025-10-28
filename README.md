# mlog

本日志库主要实现了**日期文件夹滚动日志**和***全局日志管理器***, 依赖于spdlog

### 特性:

`date_folder_rotating_sink.h`主要实现的功能:

- 兼容spdlog. 可以创建该sink用`spdlog::logger`管理

 * 每天自动创建一个新文件夹存放日志
 * 支持按大小滚动日志文件
 * 支持最大文件数量限制
 * 支持设置日志格式（pattern）
 * 支持多线程或单线程模式

`log_manager.h`实现的功能:

- 单例风格：禁止实例化和拷贝，只提供静态接口

- 每个模块可获取独立的 logger

- 支持文件日志和控制台日志（可在内部配置不同 sink）

- 支持设置全局日志级别，影响所有已创建的 logger
- 可以管理spdlog提供的`spdlog::logger`
