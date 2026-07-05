# arguments - 类型安全的命令行参数解析器

+ 名称: arguments  
+ 命名空间: `std`  
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `arguments` (独立库) |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::arguments)
```

> [!NOTE]
> `arguments` 库已公开链接到 `basic`，因此无需单独链接 `basic`。

## 描述

**arguments** 是一个受 Python `argparse` 启发的现代 C++ 命令行参数解析器。它提供类型安全的、基于模板的参数解析，具有自动类型转换和验证功能。

构建在 [`stringlist`](stringlist.md) 之上，支持多种解析风格（GNU、POSIX、Windows），并提供灵活的策略控制以实现健壮的参数处理。

## 快速开始

### 基本示例
```cpp
#include <SharedCppLib2/arguments.hpp>

int main(int argc, char** argv) {
    std::arguments args(argc, argv);
    
    // 解析字符串参数
    std::string name;
    args.addParameter("--name", name, "default");
    
    // 解析整数，支持进制
    int count;
    args.addParameter("--count", count, 10);
    
    // 解析布尔标志
    bool verbose;
    args.addFlag("--verbose", verbose);
    
    std::cout << "Name: " << name << ", Count: " << count << std::endl;
    return 0;
}
```

**使用方式:**
```bash
./program --name Alice --count 42 --verbose
# Name: Alice, Count: 42
```

## 核心功能

### 🎯 类型安全
基于模板的设计，具有自动类型推导和转换。

### 🔧 多种参数风格
- **GNU 风格**: `--option value` (默认)
- **POSIX 风格**: `-o value`
- **Windows 风格**: `/option:value`

### 📦 丰富的类型支持
支持字符串、整数、浮点数、布尔值、枚举和自定义类型。

### 🛡️ 策略控制
可配置的验证和错误处理策略。

### ℹ️ 程序名称处理
自动提取并跳过 `argv[0]`（程序名称）。

## 支持的类型

### 字符串参数
```cpp
std::string output;
args.addParameter("--output", output, "default.txt");
```

### 整数参数
```cpp
int port;
args.addParameter("--port", port, 8080);

// 支持不同进制（二进制、八进制、十六进制等）
int flags;
args.addParameter("--flags", flags, 0, 16);  // 解析为十六进制
```

### 浮点数参数
```cpp
double threshold;
args.addParameter("--threshold", threshold, 0.5);

float ratio;
args.addParameter("--ratio", ratio, 1.0f);
```

### 布尔参数
```cpp
// 带值的布尔参数
bool debug;
args.addParameter("--debug", debug, false);

// 布尔标志（存在 = true）
bool quiet;
args.addFlag("--quiet", quiet);
```

### 枚举参数
```cpp
enum class LogLevel { Debug = 0, Info = 1, Warning = 2, Error = 3 };

int level;
std::map<std::string, int> log_levels = {
    {"debug", 0}, {"info", 1}, {"warning", 2}, {"error", 3}
};
args.addEnum("--log-level", level, log_levels, 1);
```

### 自定义可反序列化类型
任何具有 `deserialize(string)` 或 `deserialise(string)` 方法的类：

```cpp
class Config {
public:
    void deserialize(const std::string& s) {
        // 从字符串解析配置
        // 例如: "key1=value1;key2=value2"
    }
};

Config config;
args.addParameter("--config", config);
```

## 参数风格

### GNU 风格（默认）
```bash
# 空格分隔（推荐）
--option value
--flag

# 等号语法（需要 AllowEqualSign 策略）
--option=value
```

### POSIX 风格
```bash
# 短选项
-o value
-f

# 组合标志（尚未实现）
-abc  # 等价于 -a -b -c
```

### Windows 风格
```bash
/option:value
/flag
```

## 解析策略

控制参数验证和行为：

```cpp
enum parse_policy {
    Null                = 0,          // 无特殊处理
    FailIfEmptyValue    = 1 << 0,     // 空值时报错
    FailIfUnknown       = 1 << 1,     // 未知选项时报错
    AllowEqualSign      = 1 << 2,     // 允许 --option=value 语法
    HelpAboutBlocking   = 1 << 3      // 检测到帮助/版本时停止并退出
};
```

### 策略使用示例
```cpp
// 严格模式：未知选项和空值都报错
std::arguments args(argc, argv, 
    FailIfEmptyValue | FailIfUnknown);

// 允许等号语法
std::arguments args(argc, argv, 
    AllowEqualSign);
```

## 函数参考

### 构造函数
```cpp
basic_arguments(int argc, CharT** argv);
basic_arguments(int argc, CharT** argv, parse_policy policy);
basic_arguments(int argc, CharT** argv, parse_policy policy, argument_style style);
```

### name
```cpp
string_type name() const;
```
返回程序名称（`argv[0]`）。下列被自动提取且不被当作选项处理。

### empty
```cpp
bool empty() const;
```
如果没有任何参数仅报改argv[0])，返回 `true`。

### addParameter（字符串）
```cpp
void addParameter(const string_type& name, 
                  string_type& value, 
                  const string_type& default_value = string_type());
```

### addParameter（整数）
```cpp
template<typename T>
requires(std::is_integral_v<T> && !std::is_same_v<T, bool>)
void addParameter(const string_type& name, 
                  T& value, 
                  T default_value = 0, 
                  int base = 10);
```
支持 2 到 36 的任意进制。

### addParameter（浮点数）
```cpp
template<typename T>
requires(std::is_floating_point_v<T>)
void addParameter(const string_type& name, 
                  T& value, 
                  std::optional<T> default_value = std::nullopt);
```

### addParameter（布尔值）
```cpp
void addParameter(const string_type& name, 
                  bool& value, 
                  bool default_value = false);
```
接受: `true/false`、`yes/no`、`on/off`、`1/0`

### addFlag
```cpp
void addFlag(const string_type& name, 
             bool& value, 
             bool default_value = false);
```
如果存在则设置为 `true`，忽略值。

### addEnum
```cpp
void addEnum(const string_type& name, 
             int& value, 
             const std::map<string_type, int>& options, 
             int default_value = 0);

// 枚举类型的模板重载
template<typename E>
requires std::is_enum_v<E>
void addEnum(const string_type& name, 
             E& value, 
             const std::map<string_type, E>& options, 
             E default_value = static_cast<E>(0));
```

模板重载自动实现枚举类型和整数类型之间的转换，无需手动转换。

### addHelp
```cpp
void addHelp(std::function<void()> helpFunction);
```
注册一个帮助处理函数，当检测到 `--help` 时调用。

```cpp
args.addHelp([]() {
    std::cout << "用法: program [选项]\n"
              << "选项:\n"
              << "  --name NAME  设置名称\n"
              << "  --help       显示此帮助消息\n";
});
```

> [!IMPORTANT]
> 尽可能尽早调用 `addHelp()`，最好在其他 `addParameter()` 调用之前。如果设置了 `HelpAboutBlocking` 策略，当检测到 `--help` 时，帮助函数将立即被调用且程序会立即退出，其他参数将不会被解析。

如果设置了 `HelpAboutBlocking` 策略，程序将在调用帮助函数后退出。

### addVersion
```cpp
void addVersion(std::function<void()> versionFunction);
```
注册一个版本处理函数，当检测到 `--version` 时调用。

```cpp
args.addVersion([]() {
    std::cout << scl2::about() << std::endl;
});
```

> [!IMPORTANT]
> 尽可能尽早调用 `addVersion()`，最好在其他 `addParameter()` 调用之前。如果设置了 `HelpAboutBlocking` 策略，当检测到 `--version` 时，版本函数将立即被调用且程序会立即退出，其他参数将不会被解析。

如果设置了 `HelpAboutBlocking` 策略，程序将在调用版本函数后退出。

### addParameter（自定义类型）
```cpp
template<typename T>
requires requires(T& t, const string_type& s) {
    requires std::is_class_v<T>;
    requires requires { t.deserialize(s); } || requires { t.deserialise(s); };
}
void addParameter(const string_type& name, 
                  T& value, 
                  std::optional<T> default_value = std::nullopt);
```

## 高级用法

### 十六进制数
```cpp
int flags;
args.addParameter("--flags", flags, 0, 16);

// 使用: --flags 0xFF 或 --flags FF
```

### 二进制数
```cpp
int mask;
args.addParameter("--mask", mask, 0, 2);

// 使用: --mask 10110101
```

### 枚举类型支持
```cpp
enum class ServiceCommand : int { 
    Null = 0, 
    Install = 1, 
    Uninstall = 2, 
    Start = 3, 
    Stop = 4, 
    Restart = 5, 
    Status = 6 
};

ServiceCommand cmd = ServiceCommand::Null;

args.addEnum("--service", cmd, {
    { "install", ServiceCommand::Install },
    { "uninstall", ServiceCommand::Uninstall },
    { "start", ServiceCommand::Start },
    { "stop", ServiceCommand::Stop },
    { "restart", ServiceCommand::Restart },
    { "status", ServiceCommand::Status }
}, ServiceCommand::Start);

// 现在可以在 switch 语句中直接使用 cmd
switch (cmd) {
    case ServiceCommand::Install:
        // 处理安装
        break;
    case ServiceCommand::Start:
        // 处理启动
        break;
    // ...
}
```

### 自定义反序列化器
```cpp
class Point {
    int x, y;
public:
    void deserialize(const std::string& s) {
        auto pos = s.find(',');
        if (pos == std::string::npos)
            throw std::invalid_argument("期望格式: x,y");
        x = std::stoi(s.substr(0, pos));
        y = std::stoi(s.substr(pos + 1));
    }
};

Point position;
args.addParameter("--pos", position);
// 使用: --pos 100,200
```

### 宽字符支持
```cpp
std::warguments args(argc, argv);

std::wstring name;
args.addParameter(L"--name", name, L"default");
```

## 错误处理

所有参数解析都可能抛出 `parameter_error`：

```cpp
try {
    std::arguments args(argc, argv, FailIfUnknown);
    
    int port;
    args.addParameter("--port", port, 8080);
    
} catch (const parameter_error& e) {
    std::cerr << "错误: " << e.what() << std::endl;
    return 1;
}
```

常见错误：
- 无效的数字格式
- 未知选项（使用 `FailIfUnknown` 策略时）
- 空值（使用 `FailIfEmptyValue` 策略时）
- 无效的枚举值
- 自定义反序列化器异常

## 最佳实践

### 1. 使用描述性名称
```cpp
// 好
args.addParameter("--output-file", output);

// 不够清晰
args.addParameter("-o", output);
```

### 2. 提供合理的默认值
```cpp
int timeout;
args.addParameter("--timeout", timeout, 30);  // 默认 30 秒
```

### 3. 对布尔值使用标志
```cpp
// 对简单的存在性检查优先使用 addFlag
bool verbose;
args.addFlag("--verbose", verbose);

// 当需要明确的 true/false 时使用 addParameter
bool enable;
args.addParameter("--enable-feature", enable, false);
```

### 4. 组合相关策略
```cpp
const auto strict_policy = FailIfEmptyValue | FailIfUnknown;
std::arguments args(argc, argv, strict_policy);
```

## 实现说明

- 使用 `std::from_chars` 进行高性能数字解析
- 类型检查零运行时开销（concepts + `if constexpr`）
- 模板函数仅在头文件中实现
- 同时支持美式拼写（`deserialize`）和英式拼写（`deserialise`）
- 基于 `scl2::basic_stringlist<CharT>` 进行高效字符串处理

## 相关文档

- [`stringlist`](stringlist.md) - 底层字符串列表实现
- [`bytearray`](bytearray.md) - 用于二进制配置格式

## 版本历史

- **v1.0.0** - 初始发布，完整模板支持
  - 任意进制的整数类型
  - 通过 `std::from_chars` 支持浮点数类型
  - 自定义可反序列化类型
  - 多种解析风格和策略
