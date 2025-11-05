# logc - 日志着色扩展

+ 名称: logc  
+ 命名空间: `logc`  
+ 文档版本: `1.0.0`  

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `logc` |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::logc)
```

## 描述

logc 是 logt 库的着色扩展，为控制台日志输出提供 ANSI 颜色支持。它通过安装预处理器函数来工作，该函数在日志消息写入输出之前向其添加颜色代码。

> [!NOTE]
> 此模块专为控制台输出设计，在记录到文件时应禁用，因为颜色代码将在文件日志中显示为原始文本。

## 与 logt 的集成

logc 通过其预处理器系统与 logt 无缝集成：

```cpp
#include <SharedCppLib2/logt.hpp>
#include <SharedCppLib2/logc.hpp>

// 安装颜色预处理器
logt::install_preprocessor(logc::logPreprocessor);
```

## 核心组件

### 颜色标准

logc 提供两种内置颜色方案：

#### 内置方案
- **DEBUG**: 浅绿色 (144, 238, 144)
- **INFO**: 白色 (255, 255, 255)  
- **WARN**: 橙色 (255, 165, 0)
- **ERROR**: 番茄红 (255, 99, 71)
- **FATAL**: 耐火砖红 (178, 34, 34)

#### VS Code 方案
- **DEBUG**: 柔和绿色 (106, 185, 112)
- **INFO**: 纯白色 (255, 255, 255)
- **WARN**: 柔和橙色 (255, 203, 107)
- **ERROR**: 亮红色 (255, 107, 107)
- **FATAL**: 深红色 (204, 62, 68)

## 函数

### logPreprocessor
```cpp
bool logPreprocessor(logt_message& message);
```
主要的预处理器函数，根据日志级别和当前颜色方案向日志消息添加颜色代码。

**返回值**: `true`（返回值当前被忽略但保留供将来使用）

### setColorStandard
```cpp
void setColorStandard(const color_standard& scheme);
```
更改活动颜色方案。使用 `logc::clrstd_buildin` 或 `logc::clrstd_vscode`。

## 使用示例

### 基本着色设置

```cpp
#include <SharedCppLib2/logt.hpp>
#include <SharedCppLib2/logc.hpp>

LOGT_MODULE("Main");

int main() {
    // 基本的 logt 配置
    logt::stdcout();
    logt::claim("Main");
    
    // 安装颜色预处理器
    logt::install_preprocessor(logc::logPreprocessor);
    
    // 可选：更改颜色方案
    logc::setColorStandard(logc::clrstd_vscode);
    
    // 彩色日志输出
    logt.debug() << "调试信息显示为绿色";
    logt.info() << "信息消息显示为白色";
    logt.warn() << "警告显示为橙色";
    logt.error() << "错误显示为红色";
    logt.fatal() << "致命错误显示为深红色";
    
    logt::shutdown();
    return 0;
}
```

### 文件/控制台输出的条件着色

```cpp
void setup_logging(bool use_colors) {
    if (use_colors) {
        // 带颜色的控制台输出
        logt::stdcout();
        logt::install_preprocessor(logc::logPreprocessor);
    } else {
        // 不带颜色的文件输出
        logt::file("application.log");
        // 不安装预处理器以获得干净的文件输出
    }
}
```

### 自定义颜色方案

```cpp
// 创建自定义颜色方案
logc::color_standard my_scheme {
    .debug = colorctl(100, 200, 100),    // 自定义绿色
    .info = colorctl(200, 200, 255),     // 浅蓝色
    .warn = colorctl(255, 200, 100),     // 金色
    .error = colorctl(255, 100, 100),    // 亮红色
    .fatal = colorctl(150, 50, 50)       // 深红色
};

// 应用自定义方案
logc::setColorStandard(my_scheme);
```

## 完整工作示例

```cpp
#include <SharedCppLib2/logt.hpp>
#include <SharedCppLib2/logc.hpp>
#include <thread>

LOGT_MODULE("MainApp");

class NetworkService {
    LOGT_DECLARE
public:
    void start() {
        logt.info() << "网络服务启动中";
        logt.debug() << "端口: 8080, 协议: HTTP";
    }
    
    void handle_error() {
        logt.error() << "发生连接超时";
    }
};

LOGT_DEFINE(NetworkService, "Network");

int main() {
    // 配置日志记录
    logt::stdcout();
    logt::claim("MainThread");
    logt::setFilterLevel(LogLevel::l_DEBUG);
    
    // 启用彩色输出
    logt::install_preprocessor(logc::logPreprocessor);
    
    logt.info() << "应用程序已初始化，启用彩色日志记录";
    
    // 演示不同日志级别的颜色
    NetworkService service;
    service.start();
    
    std::thread worker([](){
        logt::claim("WorkerThread");
        LOGT_LOCAL("Worker");
        
        logt.debug() << "工作线程详细调试信息";
        logt.info() << "工作线程处理数据中";
        logt.warn() << "检测到高内存使用率";
    });
    
    service.handle_error();
    worker.join();
    
    logt.info() << "应用程序正在关闭";
    logt::shutdown();
    return 0;
}
```

## 输出示例

启用颜色后，控制台输出将显示：
- **DEBUG** 消息显示为绿色
- **INFO** 消息显示为白色  
- **WARN** 消息显示为橙色/黄色
- **ERROR** 消息显示为亮红色
- **FATAL** 消息显示为深红色

## 重要说明

1. **文件日志记录**: 在记录到文件时禁用颜色预处理器，以避免日志文件中出现 ANSI 代码
2. **终端支持**: 颜色仅在支持 ANSI 转义码的终端中工作
3. **性能**: 颜色处理在日志工作线程中发生，而不是在调用线程中
4. **自定义**: 您可以使用 RGB 值创建完全自定义的颜色方案

## 依赖关系

- 需要 `logt` 库
- 使用 `ansiio` 生成 ANSI 颜色代码
- 基于 C++23 标准构建