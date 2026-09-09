# mlog

本日志库主要实现了**日期文件夹滚动日志**和**全局日志管理器**, 支持C++11标准, 依赖于 [spdlog](https://github.com/gabime/spdlog) 和 [filesystem](https://github.com/gulrak/filesystem).

### 实现效果

```bash
root@ubuntu:/mlog/build_output/bin# tree
.
├── logs
│   ├── 2025-10-28
│   │   ├── module1.log
│   │   └── module2.log
│   ├── 2025-10-29
│   │   ├── module1.log
│   │   └── module2.log
│   ├── 2025-10-30
│   │   ├── module1.log
│   │   └── module2.log
│   └── basicfile.log
└── main
```

### 背景:

​事情是这样的, 我在写一个服务端程序. 需要连续不断地运行. 使用spdlog的`daily_file_sink`写日志. 但是运行多天之后文件夹有很多log文件, 虽然日志使用日期命名的. 但是同一天的不同日志在文件夹中相隔很远. 查看起来不方便.于是便有了把同一天日志放在一个以日期命名的文件夹下的. 方便后续查看日志! 然后就有了该库.

### 特性:

**日期文件夹滚动日志**功能:

- 兼容spdlog. 可以创建该sink用`spdlog::logger`管理

 * 每天自动创建一个新文件夹存放日志
 * 支持按大小滚动日志文件
 * 支持最大文件数量限制
 * 支持设置日志格式（pattern）
 * 支持多线程或单线程模式

**全局日志管理器**功能:

- 单例风格：禁止实例化和拷贝以及移动，只提供静态接口
- 每个模块可获取独立的 logger
- 支持文件日志和控制台日志（可在内部配置不同 sink）
- 支持设置全局日志级别，影响所有已创建的 logger
- 可以管理spdlog提供的`spdlog::logger`
- 线程安全

### 使用方式:

说明: 项目依赖于[spdlog](https://github.com/gabime/spdlog) 和 filesystem(C++11需要[ghc::filesystem](https://github.com/gulrak/filesystem), C++17则直接使用`std::filesystem`);

1. 将[logger](logger)文件夹拷贝到项目中

2. 在顶层CMakeLists.txt中添加`add_subdirectory(logger)`;

3. 在需要使用的模块链接`target_link_libraries(<target_name> PUBLIC logger)`

4. 然后在源文件中include头文件即可使用:

   ```cpp
   #include "logger/daily_folder_rotating_sink.h"
   #include "logger/log_manager.h"
   ```

5. 推荐在创建 logger 前通过 options 集中设置日志参数：

   ```cpp
   log_manager_options options;
   options.save_path = "./logs";
   options.max_size = 100 * 1024 * 1024;
   options.max_files = 10;
   options.retention_days = 30;
   options.file_level = spdlog::level::info;
   options.stdout_level = spdlog::level::warn;

   LogManager::set_options(options);
   auto logger = LogManager::get_logger("module1");
   logger->info("hello mlog");
   ```

   原有的 `set_log_save_path`、`set_log_rotation`、`set_log_retention_days` 等接口仍然可用。

### 独立使用日期文件夹滚动 Sink

`daily_folder_rotating_sink` 是 header-only 组件，可以不使用 `LogManager`，直接通过自己的 options 创建：

```cpp
daily_folder_rotating_sink_options options;
options.base_path = "./logs";
options.log_filename = "server.log";
options.max_size = 50 * 1024 * 1024;
options.max_files = 5;
options.retention_days = 7;

auto sink = std::make_shared<daily_folder_rotating_sink_mt>(options);
```
