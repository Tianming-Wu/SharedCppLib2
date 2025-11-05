# logt - 异步日志记录库

+ 名称: logt  
+ 命名空间: 无  
+ 文档版本: `1.0.0`  

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

Logt 代表 "log threaded" - 一个高性能的异步日志记录库，适用于 C++23。它在单独的线程中执行所有日志写入操作，以防止阻塞主应用程序线程，使其非常适合性能关键的应用程序。

> [!WARNING]
> 该库使用了大量包装，仅从源代码可能难以理解。请参考本文档以正确使用。

## 主要特性

- **零阻塞设计**: 日志操作从不阻塞调用线程
- **自动线程识别**: 每个日志消息包含线程上下文
- **灵活输出**: 支持文件、std::cout 和自定义流
- **签名系统**: 类级别和模块级别的日志标识
- **高精度时间戳**: 可选的微秒级时间记录
- **可扩展**: 用于格式化和着色的自定义预处理器

## 快速开始

### 主函数中的基本用法

```cpp
#include <SharedCppLib2/logt.hpp>

LOGT_MODULE("Main");

int main() {
    logt::stdcout();  // 输出到控制台
    logt::claim("MainThread");  // 设置线程名称
    
    logt.info() << "应用程序已启动";
    logt.warn() << "这是一个警告";
    logt.error() << "发生错误: " << error_code;
    
    logt::shutdown();  // 退出前必须调用
    return 0;
}
```

### 类级别日志记录

```cpp
class DatabaseManager {
    LOGT_DECLARE
    
public:
    void connect() {
        logt.info() << "正在连接数据库...";
        logt.debug() << "连接参数: " << params;
    }
};

// 在实现文件中:
LOGT_DEFINE(DatabaseManager, "Database");
```

### 全局函数日志记录

```cpp
// network.cpp
LOGT_MODULE("Network");

void send_data() {
    logt.info() << "正在发送数据包";
    logt.debug() << "数据包大小: " << size << " 字节";
}
```

## 核心函数

### 配置方法

#### file
```cpp
static void file(const std::filesystem::path& filename);
```
设置日志输出到指定文件。文件将以追加模式打开。

#### setostream
```cpp
static void setostream(std::ostream& os);
```
将日志输出重定向到自定义输出流。

#### stdcout
```cpp
static void stdcout();
```
设置日志输出到标准输出（默认行为）。

#### claim
```cpp
static void claim(const std::string& name);
```
设置当前线程的名称。此名称将出现在该线程的所有日志消息中。

#### setFilterLevel
```cpp
static void setFilterLevel(LogLevel level);
```
设置要输出的最低日志级别。低于此级别的消息将被静默忽略。

可用级别: `l_DEBUG`, `l_INFO`, `l_WARN`, `l_ERROR`, `l_FATAL`, `l_QUIET`

#### enableSuperTimestamp
```cpp
static void enableSuperTimestamp(bool enabled);
```
启用高精度时间戳（微秒分辨率），当设置为 `true` 时。

#### install_preprocessor
```cpp
static void install_preprocessor(preprocessor_t preprocessor);
```
安装自定义预处理器函数，在输出前修改日志消息。通常与 `logc` 一起使用以获得彩色输出。

#### shutdown
```cpp
static void shutdown();
```
**必须在程序退出前调用**，以确保所有日志消息都被处理并且线程正确连接。

### 日志记录方法（通过 logt_sig）

#### info
```cpp
logt_sso info() const;
```
创建 INFO 级别的日志消息。

#### warn
```cpp
logt_sso warn() const;
```
创建 WARN 级别的日志消息。

#### error
```cpp
logt_sso error() const;
```
创建 ERROR 级别的日志消息。

#### fatal
```cpp
logt_sso fatal() const;
```
创建 FATAL 级别的日志消息。

#### debug
```cpp
logt_sso debug() const;
```
创建 DEBUG 级别的日志消息。

## 宏

### LOGT_DECLARE
在类中声明静态 logt 签名。必须放在类声明的私有部分。

### LOGT_DEFINE(Class, Name)
为类定义 logt 签名。必须放在实现文件 (.cpp) 中。

### LOGT_MODULE(Name)
为全局函数或主程序创建模块级别的 logt 签名。

### LOGT_LOCAL(Name)
在函数内创建函数局部的 logt 签名，用于临时使用。

### LOGT_TEMP(Name)
创建临时 logt 签名，用于一次性使用。

## 日志级别常量

- `l_QUIET` = -1 (抑制所有日志记录)
- `l_DEBUG` = 0
- `l_INFO` = 1  
- `l_WARN` = 2
- `l_ERROR` = 3
- `l_FATAL` = 4

## 完整示例

```cpp
#include <SharedCppLib2/logt.hpp>
#include <SharedCppLib2/logc.hpp>
#include <thread>

class DataProcessor {
    LOGT_DECLARE
public:
    void process() {
        logt.info() << "开始数据处理";
        logt.debug() << "输入大小: " << data_size;
    }
};

LOGT_DEFINE(DataProcessor, "DataProcessor");
LOGT_MODULE("MainApp");

int main() {
    // 基本配置
    logt::file("app.log");
    logt::claim("Main");
    logt::setFilterLevel(LogLevel::l_DEBUG);
    logt::enableSuperTimestamp(true);
    
    // 启用彩色输出（可选）
    logt::install_preprocessor(logc::logPreprocessor);
    
    // 日志消息
    logt.info() << "应用程序启动中";
    logt.warn() << "未找到配置文件，使用默认值";
    
    // 类使用
    DataProcessor processor;
    processor.process();
    
    // 多线程示例
    std::thread worker([](){
        logt::claim("Worker");
        LOGT_LOCAL("Worker");
        for(int i = 0; i < 5; i++) {
            logt.info() << "正在处理任务 " << i;
        }
    });
    
    worker.join();
    logt.info() << "应用程序完成";
    
    // 关键：退出前关闭
    logt::shutdown();
    return 0;
}
```

## 输出格式

使用默认设置时，日志消息遵循以下格式：
```
[YYYY/MM/DD HH:MM:SS] [级别] [线程] [签名] 消息内容
```

启用超级时间戳时：
```
[YYYY/MM/DD HH:MM:SS.MMM.UUU] [级别] [线程] [签名] 消息内容
```

其中：
- `MMM` = 毫秒（3位数字）
- `UUU` = 微秒（3位数字）

## 性能说明

- 日志操作完全非阻塞
- 繁重的格式化应在记录前完成，以最小化队列时间
- 在生产环境中考虑使用 `setFilterLevel(LogLevel::l_WARN)` 以减少日志量
- 始终调用 `shutdown()` 以防止程序退出时日志丢失

## 子模块集成

有关通过预处理器安装获得彩色输出支持，请参阅 [`logc`](logc.md)。