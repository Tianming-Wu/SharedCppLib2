# logt - 异步日志记录库

+ 名称: logt  
+ 命名空间: 无  
+ 文档版本: `1.3.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `logt` |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::logt)
```

## 描述

Logt 代表 "log threaded" - 一个专为 C++23 设计的高性能异步日志记录库。它采用单独的日志处理线程来执行所有写入操作，确保主应用程序线程永远不会被日志操作阻塞，特别适合对性能要求严格的应用程序。

> [!WARNING]
> 本库使用了复杂的包装机制，仅阅读源代码可能难以完全理解其工作原理。请务必参考本文档以正确使用各项功能。

## 主要特性

- **完全异步设计**: 所有日志操作在后台线程处理，调用线程零阻塞
- **智能线程识别**: 自动捕获并显示日志消息的线程上下文信息
- **多输出支持**: 灵活支持文件输出、标准输出和自定义数据流
- **模块化签名**: 提供类级别、模块级别和函数级别的日志标识
- **高精度计时**: 可选毫秒/微秒级时间戳精度
- **强大扩展性**: 支持自定义预处理器，实现格式化和颜色输出

## 快速开始

### 基础使用方法

```cpp
#include <SharedCppLib2/logt.hpp>

// LOGT_MODULE 用于该 cpp 文件的日志签名。不建议使用，参照 #日志签名 章节使用更合适的签名方式。
LOGT_MODULE("Main");

int main() {
    logt::stdcout(true, true);  // 设置输出到控制台
    logt::claim("MainThread");  // 命名当前线程
    
    logt.info() << "应用程序初始化完成";
    logt.warn() << "检测到非关键问题";
    logt.error() << "操作失败，错误码: " << error_code;
    
    logt::shutdown();  // 程序退出前必须调用
    return 0;
}
```

### 在类中使用日志

```cpp
class DatabaseManager {
    LOGT_DECLARE
    
public:
    void connect() {
        logt.info() << "建立数据库连接...";
        logt.debug() << "连接参数详情: " << connection_params;
    }
};

// 在对应的实现文件中定义:
LOGT_DEFINE(DatabaseManager, "Database");
```

### 函数中的日志记录

```cpp
// network.cpp 文件

void send_data() {
    LOGT_LOCAL("Network::send_data"); // 这是最清晰的签名方式，推荐使用。

    logt.info() << "开始传输数据包";
    logt.debug() << "数据包详细信息 - 大小: " << packet_size << " 字节";
}
```

### 启用调试模式
```cpp
#include <SharedCppLib2/logt.hpp>
#include <SharedCppLib2/arguments.hpp>

int main(int argc, char** argv) {
    // 此处使用 arguments 库作为示例，你可以使用任何方式解析。
    std::arguments args(argc, argv);

    // ... 其他日志初始化代码 ...

    if(args.addFlag("debug")) {
        logt::setFilterLevel(LogLevel::Debug);  // 启用调试日志输出
    }

    LOGT_LOCAL("main"); // 主函数局部签名
    logt.debug() << "调试模式已启用";

    logt::shutdown();
    return 0;
}
```

### 在无法继续执行时退出
```cpp

void some_critical_function() {
    LOGT_LOCAL("CriticalFunction");

    if(!initialize_critical_resource()) {
        logt.fatal() << "无法初始化关键资源，程序将退出";
        // 直接退出程序
        // 使用 logt::exit() 可以确保日志系统正确清理并刷新所有日志消息。
        logt::exit(EXIT_FAILURE);
    }

    // ... 其他逻辑 ...
}
```


### logt_guard - RAII 关闭辅助

```cpp
logt_guard guard;  // 析构时自动调用 logt::shutdown()
```

一个便捷的 RAII 包装类，其析构函数自动调用 `logt::shutdown()`。在合适的作用域（例如 `main()` 中）声明一个 `logt_guard` 实例，即可确保离开作用域时日志系统被正确关闭。

> [!NOTE]
> `logt_guard` 仅为便利性提供，完全可选。如果你选择不使用它，则**必须**在程序退出前调用 `logt::shutdown()`，并且必须在最后一个退出的线程中调用。未调用 `shutdown()` 会在 `main()` 退出时触发 `std::terminate()`，因为工作线程仍在运行。

**示例：**

```cpp
#include <SharedCppLib2/logt.hpp>

LOGT_MODULE("Main");

int main() {
    logt_guard guard;  // shutdown() 将自动调用

    logt::stdcout(true, true);
    logt::claim("MainThread");

    logt.info() << "应用程序初始化完成";

    // 无需显式调用 logt::shutdown()
    return 0;
}
```

## 核心 API 参考

### 系统配置方法

#### addfile - 添加文件通道
```cpp
static int addfile(const std::filesystem::path& filename, bool default_enable = true);
```
以追加模式打开文件通道并返回通道 ID。`default_enable` 决定新签名是否默认启用该通道。若通道数量耗尽返回 `-1`。

#### addostream - 添加自定义流
```cpp
static int addostream(std::ostream& os, bool default_enable = true);
```
注册自定义输出流并返回通道 ID。传入的流对象需在日志期间保持有效。logt 不负责销毁该对象。

#### stdcout - 控制台通道
```cpp
static void stdcout(bool enable = true, bool default_enable = true);
```
控制通道 0（stdout）。该通道始终存在，可选择禁用或不作为默认通道。

#### claim - 线程命名
```cpp
static void claim(const std::string& name);
```
为当前线程设置可读名称；后续该线程的日志会显示此名称。否则，该处会显示该线程的线程ID。

#### setFilterLevel - 日志过滤
```cpp
static void setFilterLevel(LogLevel level);
```
设置全局最低日志级别。级别：`LogLevel::Debug`, `LogLevel::Info`, `LogLevel::Warn`, `LogLevel::Error`, `LogLevel::Fatal`，特殊 `LogLevel::Quiet`。

#### setChannelFilter - 按通道日志过滤
```cpp
static void setChannelFilter(int channel_id, LogLevel level);
```
为特定通道设置独立的过滤级别，覆盖全局 `setFilterLevel()` 设置。使用 `LogLevel::Inherit` 恢复为全局过滤级别。当需要不同通道记录不同严重程度的日志时非常有用——例如控制台通道只显示警告，而文件通道记录全部信息。

#### enableSuperTimestamp - 高精度时间戳
```cpp
static void enableSuperTimestamp(bool enabled);
```
启用后时间戳包含毫秒/微秒。

#### install_preprocessor - 预处理器
```cpp
static void install_preprocessor(preprocessor_t preprocessor);
```
安装预处理钩子。预处理现位于 `write_message()` 中：可对 stdout/自定义流保留彩色内容，同时文件通道自动使用原始未处理内容（避免 ANSI 写入文件）。

#### shutdown - 系统关闭
```cpp
static void shutdown();
```
优雅停止工作线程并刷新队列中的日志。**程序退出前务必调用。** 必须在最后一个退出的线程中调用。未调用 `shutdown()` 会在 `main()` 退出时触发 `std::terminate()`。

#### logt_guard - RAII 关闭守卫
```cpp
class logt_guard {
public:
    logt_guard() = default;
    ~logt_guard() { logt::shutdown(); }
};
```
一个简单的 RAII 包装类，析构时自动调用 `shutdown()`。这是手动调用 `logt::shutdown()` 的便利替代方案。使用 `logt_guard` 后无需再显式调用 `shutdown()`。

### 日志记录方法

通过 logt_sig 对象提供的各级别日志记录方法：

#### info - 信息级别
```cpp
logt_sso info() const;
```
创建 INFO 级别的日志消息流，用于记录常规应用程序运行信息。

#### warn - 警告级别
```cpp
logt_sso warn() const;
```
创建 WARN 级别的日志消息流，用于记录可能需要关注的异常情况。

#### error - 错误级别
```cpp
logt_sso error() const;
```
创建 ERROR 级别的日志消息流，用于记录错误条件，但不影响程序继续运行。

#### fatal - 严重级别
```cpp
logt_sso fatal() const;
```
创建 FATAL 级别的日志消息流，用于记录导致程序无法继续运行的严重错误。

#### debug - 调试级别
```cpp
logt_sso debug() const;
```
创建 DEBUG 级别的日志消息流，用于记录详细的调试信息，通常在开发阶段使用。注意，在默认配置下该级别的消息总是会被丢弃。

## 宏定义参考

注：目前实现原因，仅推荐使用 `LOGT_LOCAL` 定义，阅读章节 [日志签名](#日志签名) 了解更多。

在程序主入口点，建议将 LOGT_LOCAL 放到通道初始化代码之后，否则可能因为重用优化而导致通道配置错误。相关功能正在重构中，未来版本将支持更灵活的日志签名定义方式。

### LOGT_DECLARE
在类声明内部声明静态日志签名对象。必须放置在类的私有访问区域。

### LOGT_PUBLIC_DECLARE
公开版本的 `LOGT_DECLARE`，用于暴露静态日志签名以便外部配置（例如调整通道）。

### LOGT_DEFINE(Class, Name)
在实现文件中为特定类定义日志签名。Class 参数指定目标类名，Name 参数定义该类的日志标识符。

### LOGT_MODULE(Name)
创建模块级别的日志签名，适用于全局函数、命名空间作用域或主程序入口。

### LOGT_LOCAL(Name)
创建函数作用域内的局部日志签名，适用于在函数内部临时使用的日志标识。

### LOGT_TEMP(Name)
创建临时日志签名对象，适用于一次性使用的日志场景，不保留静态状态。

## 通道管理

- **通道 0**：stdout 永远注册，可通过 `stdcout(enable, default_enable)` 控制。
- **添加通道**：`addfile()` / `addostream()` 返回通道 ID，并设置对新签名的默认启用状态。后续可以通过创建时返回的通道 ID 来管理通道。不会添加类似名称索引的通道标识，因为它会拖慢底层速度，如有需要请自行管理通道 ID 与其用途的映射。
- **按签名控制**：`logt_sig::setChannel(id, enable)` / `setChannels({ids...})` 返回 `bool`，会校验通道是否已注册。
- **公开访问**：若类需要外部修改通道，可使用 `LOGT_PUBLIC_DECLARE` 暴露静态日志对象。
- **预处理作用域**：预处理发生在 `write_message()`；stdout/自定义流使用处理后的内容（可含颜色），文件通道使用原始内容（无 ANSI 码）。

## 日志级别详解

- `LogLevel::Quiet` = -1 — 完全静默模式，不记录任何日志
- `LogLevel::Debug` = 0 — 调试级别，记录最详细的运行信息
- `LogLevel::Info` = 1 — 信息级别，记录常规运行状态
- `LogLevel::Warn` = 2 — 警告级别，记录可能的异常情况
- `LogLevel::Error` = 3 — 错误级别，记录错误条件
- `LogLevel::Fatal` = 4 — 严重级别，记录致命错误
- `LogLevel::Inherit` = 16 — 通道过滤的保留标记值，表示使用全局设置

## 完整应用示例

```cpp
#include <SharedCppLib2/logt.hpp>
#include <SharedCppLib2/logc.hpp>
#include <thread>

class DataProcessor {
public:
    void process() {
        LOGT_LOCAL("DataProcessor::process");

        logt.info() << "启动数据处理流程";
        logt.debug() << "输入数据集大小: " << input_data.size();
        // 处理逻辑...
    }
};

int main() {
    // 系统初始化配置
    int file_channel = logt::addfile("application.log");  // 输出到文件
    logt::stdcout(true);              // stdout 通道 0
    logt::claim("MainThread");       // 主线程命名
    logt::setFilterLevel(LogLevel::Debug);  // 设置日志级别
    logt::enableSuperTimestamp(true);  // 启用高精度时间戳
    // 将模块日志路由到 stdout + 文件
    logt.setChannels({0, file_channel});

    LOGT_LOCAL("main"); // 主函数局部签名
    
    // 颜色预处理：stdout/自定义流保留颜色，文件保持无颜色文本
    // logt::install_preprocessor(logc::logPreprocessor);
    
    // 记录启动信息
    logt.info() << "主应用程序初始化完成";
    logt.warn() << "使用默认配置参数";
    
    // 使用类日志功能
    DataProcessor data_processor;
    data_processor.process();
    
    // 多线程日志示例
    std::thread worker_thread([](){
        logt::claim("WorkerThread");  // 工作线程命名
        LOGT_LOCAL("Worker");         // 局部日志签名
        
        for(int task_id = 0; task_id < 5; task_id++) {
            logt.info() << "执行工作任务编号: " << task_id;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    
    worker_thread.join();
    logt.info() << "所有处理任务已完成";
    
    // 关键步骤：关闭日志系统
    logt::shutdown();
    return 0;
}
```

## 日志签名
日志签名是 logt 中用于标识日志消息来源的字符串标签。它们可以在类、模块或函数级别定义，以便在日志输出中清晰地显示消息的上下文来源。

但是，由于目前的设计问题，暂时不建议使用 `LOGT_MODULE`、`LOGT_DECLARE` 等日志签名方式，因为它们所标记的范围较广，不利于定位问题。建议直接在函数内部使用 `LOGT_LOCAL` 定义局部日志签名，以实现精准定位和更清晰的日志输出。


## 输出格式说明

标准输出格式：
```
[年/月/日 时:分:秒] [级别] [线程名] [签名] 日志消息内容
```

启用高精度时间戳后的格式：
```
[年/月/日 时:分:秒.毫秒.微秒] [级别] [线程名] [签名] 日志消息内容
```

格式说明：
- 时间戳: 完整的日期和时间信息
- 级别: 日志的严重程度标识（DEBUG/INFO/WARN/ERROR/FATAL）
- 线程名: 产生日志的线程标识名称
- 签名: 日志消息的来源模块或类标识
- 消息内容: 用户提供的具体日志信息

## 性能优化建议

- **零阻塞优势**: 充分利用异步特性，日志操作不会影响主线程性能
- **预处理优化**: 复杂的字符串拼接和格式化建议在日志调用前完成，减少队列中的处理时间
- **生产环境配置**: 在生产环境中建议设置 `setFilterLevel(LogLevel::Warn)` 或更高，减少不必要的日志输出
- **资源清理**: 务必在程序退出前调用 `shutdown()` 方法，防止日志消息丢失和资源泄漏

## 扩展功能集成

通过预处理器机制，logt 可以与其他功能模块无缝集成。特别是与 [`logc`](logc.md) 颜色输出库配合使用，可以为终端日志添加丰富的颜色标识，提升日志可读性。预处理器还支持自定义格式转换、敏感信息过滤、日志审计等高级功能。