# ini - INI 配置解析库

+ 名称: ini
+ 命名空间: （无）
+ 文档版本: `1.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `ini` |

包含方式:
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(my_target PRIVATE SharedCppLib2::ini)
```

## 描述

`ini` 提供了轻量且易用的 INI 配置文件解析与写入能力。支持常见的字符串、整型、浮点、布尔、`std::stringlist` 与 `std::bytearray` 类型；并为不同使用场景提供三种取值接口：带默认值参数、使用类型默认构造值，以及返回 `std::optional<T>` 的可选版本。

## 快速开始

### 基本读取/写入
```cpp
#include <SharedCppLib2/ini.hpp>

int main() {
		ini cfg;
		cfg.loadFromFile("config.ini");

		// 读取（字符串）
		std::string name = cfg.getValue("User", "Name", "default_name");

		// 修改并保存
		cfg.setValue("User", "Name", "alice");
		cfg.saveToFile("config.ini");
}
```

### 整数与浮点读取
```cpp
int w = cfg.getValueAsInteger<int>("Window", "Width", 800);
double ratio = cfg.getValueAsFloat<double>("Display", "Scale", 1.0);
```

## 三种取值风格（重要说明）

为适应不同编码风格和错误处理需求，`ini` 同时提供三类接口：

- 使用 **显式默认值** 的版本（推荐用于需要自定义默认行为的场景）
	- 示例: `cfg.getValue("S", "K", "fallback")` 或 `cfg.getValueAsInteger<int>("S","K", 42)`。
	- 优点：调用处明确指定默认值，易读且安全。

- 使用 **类型默认构造函数** 的版本（通过省略默认参数）
	- 对于模板函数（整型/浮点），默认参数为 `T()`；对于 `getValue` 字符串重载，默认为空字符串 `""`。
	- 示例: `int n = cfg.getValueAsInteger<int>("S","K");`  // 未找到或解析失败时返回 `0`
	- 优点：调用更简洁，适用于接受类型缺省值的场景。

- 返回 `std::optional<T>` 的 **可选版本**（当需要区分“未设置”与“解析为默认值”时使用）
	- 函数名通常以 `Optional` 或 `...Optional` 结尾，例如：
		- `std::optional<std::string> getValueOptional(const std::string& section, const std::string& key) const;`
		- `std::optional<int> getValueAsIntegerOptional<int>(...)`。
	- 示例:
		```cpp
		auto maybeV = cfg.getValueOptional("S", "K");
		if (maybeV.has_value()) { // 注意，虽然写作 if (maybeV) 可行，但在包含值为 bool 时可能引起歧义
				// 存在，使用 *maybeV
		} else {
				// 未设置
		}
		```
	- 优点：能明确区分配置项不存在与存在但值为空/解析失败的情况。

## 布尔值约定

布尔读取支持以下文本表示（不区分大小写）：

- 真值: `1`, `true`, `yes`, `on`
- 假值: `0`, `false`, `no`, `off`

示例：
```cpp
bool enabled = cfg.getValueAsBool("Feature", "Enable", false);
auto maybeEnabled = cfg.getValueAsBoolOptional("Feature", "Enable");
```

## stringlist 与 bytearray 支持

- `getValueAsStringList(...)` / `getValueAsStringListOptional(...)`：用于解析由 `stringlist::pack()` 生成的字符串列表表示。
- `getValueAsByteArray(...)` / `getValueAsByteArrayOptional(...)`：用于解析十六进制编码（由 `std::bytearray::tohex()` 生成）的字节数组。

示例：
```cpp
std::stringlist items = cfg.getValueAsStringList("Section", "Items");
auto maybeBytes = cfg.getValueAsByteArrayOptional("Bin", "Data");
```

## API 参考（常用函数）

- `bool loadFromFile(const std::filesystem::path& filename);`
- `bool saveToFile(const std::filesystem::path& filename) const;`
- `std::string getValue(const std::string& section, const std::string& key, const std::string& default_value = "") const;`
- `template<typename T> T getValueAsInteger(const std::string& section, const std::string& key, const T& default_value = T()) const;`
- `template<typename T> T getValueAsFloat(const std::string& section, const std::string& key, const T& default_value = T()) const;`
- `bool getValueAsBool(const std::string& section, const std::string& key, bool default_value = false) const;`
- `std::optional<std::string> getValueOptional(const std::string& section, const std::string& key) const;`
- `template<typename T> std::optional<T> getValueAsIntegerOptional(const std::string& section, const std::string& key) const;`
- `std::stringlist getValueAsStringList(const std::string& section, const std::string& key) const;`
- `std::bytearray getValueAsByteArray(const std::string& section, const std::string& key) const;`

（完整接口请参考 `include/ini.hpp`）

## 实际示例

### 使用默认值
```cpp
int threads = cfg.getValueAsInteger<int>("App", "Threads", 4);
```

### 使用类型默认值（省略默认参数）
```cpp
int timeout = cfg.getValueAsInteger<int>("Network", "Timeout"); // 若不存在返回 0
```

### 使用 std::optional 判断是否存在
```cpp
auto maybeUser = cfg.getValueOptional("User", "Name");
if (maybeUser) {
		std::cout << "User: " << *maybeUser << std::endl;
} else {
		std::cout << "User 未设置\n";
}
```

## 注意事项

- INI 中的 Section 与 Key 名是区分大小写的。
- 对于可能缺失的配置项，优先考虑使用 `std::optional` 版本以便明确区分“未设置”与“默认值”两种语义。
- `getValueAsInteger` / `getValueAsFloat` 在解析失败时会返回指定的默认值；若解析失败且使用可选版本则返回 `std::nullopt`。

---

更多示例请参阅仓库内的测试：`SharedCppLib2Test` 目录下的相关测试用例。

