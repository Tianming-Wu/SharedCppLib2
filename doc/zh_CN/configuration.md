# 配置方案文档

本文档概述了 SharedCppLib2 中的配置系统方案。

需要明确的是，SharedCppLib2 并没有内置的配置系统，但它提供了一套工具和接口来帮助你构建自己的配置系统。

以下是你可以选择的方案。

如果难以抉择，简单的场景使用 ini，处理大量二进制数据请使用 keydb。



# 快速开始

## 基于文本的配置

SharedCppLib2 提供了一些简单的基于文本的配置格式。

不，不支持 json。请使用 [jsoncpp](https://github.com/open-source-parsers/jsoncpp)。

**支持的格式：**
- [Ini 格式](#ini-格式)
- [Xml 格式](#xml-格式)

### Ini 格式

你可以使用 [`ini`](./ini.md) 模块轻松管理配置。

```cpp
#include <SharedCppLib2/ini.hpp>

int main() {
    ini config;

    if(!config.loadFromFile("config.ini")) {
        std::cerr << "无法加载配置文件。" << std::endl;
        return 1;
    }

    std::string name = config.getValue("main", "name", "default_name");
    config.setValue("main", "version", "1.0.0");

    int i = config.getValueAsInteger<int>("main", "max_connections", 100);

    // setValue 支持任何可以使用 std::to_string 转换为字符串的值，也直接支持字符串。

    // 布尔值也支持。
    bool debug_mode = config.getValueAsBool("main", "debug_mode", false);
    config.setValue("main", "debug_mode", !debug_mode);

    // Ini 库还支持 SharedCppLib2 API 集成。这里不展开说明，更多信息请查看 ini 文档。


    // 你可以使用可选的系列函数来检查值是否存在。不过它会让代码变得更复杂。
    auto w = config.getValueAsIntegerOptional<int>("main", "window_width");
    if(w.has_value()) {
        std::cout << "窗口宽度：" << w.value() << std::endl;
    } else {
        std::cout << "未找到窗口宽度，使用默认值。" << std::endl;
    }

    config.saveToFile("config.ini");
    return 0;
}
```

### Xml 格式
没错，这个库确实包含一个 XML 实现。不过 xml 对于配置来说通常过于复杂了。但如果你确实需要用它来处理某些动态的结构化数据，请查看 xml 模块的文档。


## 基于 Bytearray 的配置

### KeyDB 配置

SharedCppLib2 提供了一个简单的键值数据库 KeyDB，可用于配置存储。

```cpp
// 示例尚未完成，更多详情请查看 KeyDB 文档。
```

### Bytearray 配置

SharedCppLib2 设计了一套标准的配置存储流程。

这是为了让整个过程清晰、可控且高效。所有工作都由模块本身处理，使得顶层代码简单到只需 1-2 行（未压缩的情况下）。

它允许你将任意类型的数据转换为字节数组。生成的字节数组可以存储在文件中、通过网络发送、或加密等。


下面的示例使用 struct 来定义配置。class 也可以，在 C++ 中它们基本一样。
唯一的区别是，对于 class，你**必须**将 load 和 dump 函数设为公开。

如果你需要为 struct 编写构造函数，请确保同时保留一个空构造函数（可以是私有的），因为加载时可能需要先构造一个空的 struct。不过，如果你的 load 函数不依赖于此，则不必这样做。

#### 定义纯配置结构体

纯配置结构体是指只包含平凡数据类型的结构体，即它们可以被直接复制而无需特殊处理。例如：
```cpp
struct MyConfig {
    int window_width;
    int window_height;
    bool debug_mode;
};
// 该结构体被认为是纯的，因为它只包含平凡数据类型（int 和 bool）。
```
纯结构体直接受支持，但你需要手动构造 bytearray，因为这部分 API 仍在开发中。不过它仍然很简单。

```cpp
myConfig cfg{800, 600, true};
std::bytearray ba(cfg); // 就这么简单。

myConfig cfg2 = ba.as<MyConfig>(); // 同样简单。
```

如果是用于内嵌场景，你也可以为结构体编写 load 和 dump 函数。唯一的区别在于 load 函数中需要指定大小：
```cpp
myStruct myStruct::load(const std::bytearray_view& data) {
    return data.readBytes(sizeof(myStruct)).as<myStruct>();
}
```
> 同样，这一步是因为 API 尚未完善。最终版本的 API 将简化此过程，让你无需编写任何代码即可处理。

#### 定义非纯配置结构体

例如，如果你有一个可以用作配置的结构体：
```cpp
struct MyConfig {
    std::string name;
    bool debug_mode;
    LogLevel log_level;
};
```

为了让 API 正常工作，建议使用以下方法：
```cpp
struct MyConfig {
    // ... 之前的定义 ...

    static MyConfig load(const std::bytearray_view& data);
    static std::bytearray dump(const MyConfig& config);
};

// 你可以使用这个宏来断言你是否正确编写了 API：
scl2_check_generic_dump_load(MyConfig);
```
注意这里使用的是 bytearray_view 而不是 bytearray，因为 bytearray_view 为你处理了游标，是使代码简洁易读的关键部分。

然后你需要实现 load 和 dump 函数。

你唯一需要确保的是这些函数之间的一致性。也就是说，如果你 dump 一个配置然后 load 回来，应该总是得到相同的配置。你必须自己确保这一点，因为无论如何我都无法替你处理。

```cpp

MyConfig MyConfig::load(const std::bytearray_view& data) {
    MyConfig config;

    // 对于字符串，使用提供的 readString。
    // 你应该将 readString() 和 addString() 成对使用。
    config.name = data.readString();

    // 对于宽字符串，还有 readWString 和 addWString。
    // 所有其他字符串类型如 u8string 和 u16string 不受
    // 视图 API 支持，但受 bytearray API 支持。你可以根据需要手动处理它们。

    // 对于大多数普通类型，使用模板 read。
    config.debug_mode = data.read<bool>();
    config.log_level = data.read<LogLevel>();

    return config;
}

std::bytearray MyConfig::dump(const MyConfig& config) {
    std::bytearray ba;

    // 你可以选择预分配 bytearray。语法与 STL 容器相同。
    ba.reserve(256); // 这不是必须的，但可以减少内存分配次数，提高性能。
    
    // 对于字符串，使用提供的 addString。此函数会为你处理字符串长度。
    ba.addString(config.name);

    // 对于大多数数据类型，尤其是数字类型（包括枚举），你可以直接将它们追加到 bytearray。
    ba.append(config.debug_mode);
    ba.append(config.log_level);

    return ba;
}
```

#### 定义内嵌配置结构体
内嵌配置结构体是指包含其他配置结构体的结构体。例如：
```cpp
struct AppConfig {
    MyConfig myConfig;
    scl2::logmgr_config logmgrConfig;

    // 在这里以相同的方式定义 load 和 dump。
};
```

以下是精彩的部分。load/dump 函数可以对子结构体递归调用。
```cpp
AppConfig AppConfig::load(const std::bytearray_view& data) {
    AppConfig config;

    // 只需调用子结构体的 load 函数，它就会为你处理一切。
    // 不过仍然需要按照固定的顺序调用。
    config.myConfig = MyConfig::load(data);
    config.logmgrConfig = scl2::logmgr_config::load(data);

    return config;
}

std::bytearray AppConfig::dump(const AppConfig& config) {
    std::bytearray ba;

    // 只需调用子结构体的 dump 函数，它就会为你处理一切。
    ba.append(MyConfig::dump(config.myConfig));
    ba.append(scl2::logmgr_config::dump(config.logmgrConfig));

    return ba;
}
```


#### Bytearray 处理
以下是你的顶层代码应如何与上述 API 交互。

##### 通用方法
除了一些为简化流程而设计的单行 API 外，以下是处理配置的通用方法。以传统的文件方法为例：

```cpp
#include <SharedCppLib2/api.hpp>
#include <fstream>

int main() {
    {
        // 对于任何读取 API 正常工作，你**必须**以二进制模式打开文件。
        std::ifstream cfgf("config.db", std::ios::binary);

        std::bytearray ba;
        cfgf >> ba;

        // 这里不太清楚，但**无论如何**，你必须让当前作用域拥有
        // 加载后的 bytearray，并确保它在你完成配置加载之前保持有效生命周期。
        // 这是因为 bytearray_view 不拥有数据，你有责任确保数据在
        // 你完成配置加载之前保持有效。
        MyConfig config = MyConfig::load(ba);

        // 如果你不喜欢临时的 bytearray 对象，可以在使用后清除它。
        ba.clear();
    }

    {
        std::bytearray outba = MyConfig::dump(config);
        std::ofstream outcfgf("config.db", std::ios::binary);
        outcfgf << outba;
    }
}
```

##### 文件操作
你可以使用 SharedCppLib2 文件 I/O 单行 API 来处理文件读取和写入。

```cpp
#include <SharedCppLib2/fileio.hpp>

// 建议使用 "db" 后缀，因为看起来很酷 (

MyConfig config = scl2::readAndLoad<MyConfig>("config.db"); // 这需要支持 "load"。

scl2::writeFile("config.db", config); // 这需要支持 "dump"。

// 建议使用文件绝对路径，因为你可能不希望配置文件位于当前工作目录中。
// 你可以使用平台 API 来实现：

#include <SharedCppLib2/platform.hpp>
std::filesystem::path configPath = platform::executable_dir() / "config.db";
```

##### 网络
至于网络部分，基本上是"你想怎么做就怎么做"。SharedCppLib2 确实提供了网络 API，但你目前可能还看不到，因为我决定在完成所有功能并通过充分测试之前不发布它们。


##### 任意基于流的操作
SharedCppLib2 实际上提供了流定义，请查看 `stream.hpp`。

你应该注意到，受支持的流使用 bytearray。这太简单了，我就不在这里赘述了。

一个好的示例可以参考 [LibPipe](https://github.com/Tianming-Wu/LibPipe) 项目。


##### 加密
SharedCppLib2 还提供了一套加密 API 定义，包含在 SharedCppLib2 通用 API 中。

但是，目前 SharedCppLib2 中还没有实现任何加密算法。对于用户编写的任何满足要求的类（有关更多详细信息，请参阅加密 API 文档），应该很简单：

```cpp
#include <SharedCppLib2/api.hpp>
#include <AES256.hpp> // 这是一个占位示例，代表支持的加密算法。

std::bytearray cfgData = MyConfig::dump(config);

std::bytearray enc = scl2::encrypt<AES256>(cfgData, key);
scl2::writeFile("config.enc", enc);

std::bytearray uecfgData = scl2::decrypt<AES256>(enc, key);

// ...
```
