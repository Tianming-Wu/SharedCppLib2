# serial - 串口

+ 名称: serial
+ 命名空间: `scl2`
+ 文档版本: `1.0.0`

## CMake 配置信息

| 项目 | 值 |
|---------|---------|
| 命名空间 | `SharedCppLib2` |
| 库名称 | `serial` |
| 依赖 | `basic`（Windows 另需 `platform` 与 `advapi32`，均为私有依赖） |

引入方式：
```cmake
find_package(SharedCppLib2 REQUIRED)
target_link_libraries(target SharedCppLib2::serial)
```

```cpp
#include <SharedCppLib2/serial.hpp>
```

## 简介

`scl2::serial` 就是一个打开着的串口，它是 `scl2::basic_iostream`。因此它直接加入了库里
所有“流”的用法：`read()` 取字节、`write()` 发字节、`available()` 报告还有多少在等、
`waitForReadyRead()` 等到有数据为止。

串口和管道、套接字最大的不同，也决定了整个 API 的形状：**没有对端，也没有消息边界**。
端口本身就是连接，所以只有一个对象而不是 server / client 两半；一次读是“若干字节”，
而不是“一条消息”。

Windows（COMM API）与 POSIX（termios）在同一份头文件后面都实现了。头文件里不出现任何
平台类型——原生句柄以不透明的 `std::intptr_t` 传递，和 `process.hpp` 的做法一样——所以
可移植代码只管 include 并链接目标。

## 快速上手

```cpp
#include <SharedCppLib2/serial.hpp>

scl2::serial port;

if (!port.open("COM3")) {                       // 默认 115200 8N1，无流控
    std::fprintf(stderr, "打开失败: %s\n", port.lastError().c_str());
    return;
}

port.write(scl2::bytearray("AT\r\n"));

if (port.waitForReadyRead(std::chrono::seconds(1))) {
    scl2::bytearray reply = port.readAll();
}
```

POSIX 上名字是设备节点：`port.open("/dev/ttyUSB0")`。

## 打开与关闭

| 调用 | 含义 |
|------|------|
| `open(name, settings)` | 以这组线路参数占用端口；替换对象原先持有的内容 |
| `close()` / `reset()` | 释放端口；幂等，总是返回 `true` |
| `isOpen()` / `valid()` | 当前是否持有端口 |
| `portName()` | 打开时给的端口名，按原样保留 |
| `nativeHandle()` | 背后的 `HANDLE` 或文件描述符 |

Windows 以不共享的方式打开，别的程序占着的端口打不开。打开失败时名字也会保留，
所以 `portName()` 和 `lastError()` 能说清试的是什么、为什么没成。

## 线路参数

```cpp
scl2::serial::settings config;
config.baud_rate = 921600;
config.data_bits = 8;
config.parity_bit = scl2::serial::parity::Even;
config.stop = scl2::serial::stop_bits::One;
config.flow = scl2::serial::flow_control::None;

port.open("COM4", config);
port.applySettings(config);          // 之后改参数，不用重新打开
scl2::serial::settings current = port.currentSettings();
```

| 字段 | 取值 | 说明 |
|------|------|------|
| `baud_rate` | 任意正整数 | POSIX 只接受它那些常量里有名字的速率 |
| `data_bits` | 5 .. 8 | |
| `parity_bit` | `None`、`Odd`、`Even`、`Mark`、`Space` | mark / space 需要 Windows 或 Linux |
| `stop` | `One`、`OnePointFive`、`Two` | `OnePointFive` 仅 Windows |
| `flow` | `None`、`Hardware`（RTS/CTS）、`Software`（XON/XOFF）、`Both` | |

平台表达不了的参数，`applySettings()` 返回 `false` 并写好 `lastError()`——例如 POSIX 上的
1.5 位停止位或 mark 校验——端口保持原有参数。

## 超时

| 读超时 | `read()` 会等 |
|--------|----------------|
| 正数（默认 1 秒） | 等这么久凑齐请求的字节数，然后返回已经到达的部分 |
| `0` | 完全不等：把已经排队的数据取走 |
| 负数 | 不限时 |

| 写超时 | Windows 上 `write()` 会等 | POSIX 上 |
|--------|---------------------------|----------|
| 正数（默认 1 秒） | 等这么久 | 等这么久 |
| `0` / 负数 | 不限时 | 不等 / 不限时 |

这个不对称是驱动带来的：Windows 的 COMM API 没有“立刻返回”的写法，所以那里的 0 只能表示
“不设超时”。

写到超时的时候，返回的是驱动收下的字节数——可能一个都没有——`lastError()` 不会变，因为
并没有出错。拿返回值和传入的大小比一下，就知道该从哪里接着发。

## 读写

| 调用 | 行为 |
|------|------|
| `read(n)` | 最多等读超时，返回到达的数据，至多 `n` 个字节 |
| `readAll()` | 取走当前排队的一切，不等 |
| `write(data)` | 返回驱动收下的字节数；部分写入说明该从哪里接着发 |
| `waitForReadyRead(t)` | 有数据可读、或端口已经消失时返回 `true` |
| `waitForBytesWritten(t)` | 驱动的发送队列空了返回 `true` |

串口没有可等待的句柄，`waitForReadyRead()` 每 2 毫秒轮询一次驱动。这是唯一一处“等待的精度
不如超时值看起来那么好”的地方。

`waitForReadyRead()` 只保证**有数据到了**，不保证你等的全部到齐：回环测试里写完 4096 字节，
它在第一个字节回来时就返回 `true`。要凑够确定的字节数，用 `read(n)`，或者自己循环到
`available()` 满足为止。

## 控制线

| 调用 | 含义 |
|------|------|
| `purge(purge_target::Receive/Transmit/Both)` | 扔掉驱动里排队的收发数据 |
| `bytesToWrite()` | 还有多少字节在发送队列里（问不到时为 0） |
| `modem()` | 输入线，`modem_lines` 的 `cts` / `dsr` / `ri` / `dcd` |
| `setDtr(bool)` / `setRts(bool)` | 手动驱动输出线 |
| `sendBreak(duration)` | 维持一段时间的 break 条件 |
| `waitForBytesWritten(t)` | 等发送队列清空 |

Windows 上硬件流控会自己接管 RTS，所以 `flow_control::Hardware` 或 `Both` 生效时
`setRts()` 会被驱动覆盖掉。`sendBreak()` 在 Windows 上会用上 duration；POSIX 发的是系统
自己的 break，时长参数被忽略。

## 端口名与枚举

| 平台 | 名字 | `availablePorts()` |
|------|------|--------------------|
| Windows | `COM3`（COM10 及以上需要的 `\\.\` 前缀会自动补上） | 注册表里的设备映射 |
| POSIX | `/dev/ttyUSB0` | 扫 `/dev` 下的常见设备名 |

`serial::availablePorts()` 是静态函数，返回自然排序的列表（`COM2` 在 `COM10` 前面）。
`serial::portExists(name)` 回答端口名是否存在，但不打开、也不占用它。

## 失败处理

什么都不抛。失败的调用返回 `false`（或者读到不足、写入 0 字节），并记下原因：

```cpp
if (!port.open("COM9")) {
    std::fprintf(stderr, "%u: %s\n", port.lastErrorCode(), port.lastError().c_str());
}
```

| 调用 | 含义 |
|------|------|
| `lastError()` | 最后一次失败的原因，用系统的原话；没失败过时为空 |
| `lastErrorCode()` | 背后的 Win32 错误码或 `errno`；0 表示是本模块自己判定的失败 |
| `broken()` | 是否有操作已经发现端口消失 |

`broken()` 回答的正是“刚才是超时，还是设备没了”。被拔掉的 USB 串口无法再等、也无法重试，
所以一旦有操作看见它，`readable()` 就继续返回 `true`、读操作返回空——调用方能察觉到，
而不是永远等下去。`open()` 会清掉这个标志。

Windows 上被拔掉的转换器，下一次操作会报 `拒绝访问`（错误码 5），`availablePorts()` 也不再
列出它；在那之后碰到这个端口的每个调用都会带着同一个错误返回 `false`（或空），直到关闭为止。

## 平台差异

| 主题 | Windows | POSIX |
|------|---------|-------|
| 后端 | COMM API（`CreateFile`、`DCB`、`COMMTIMEOUTS`） | `termios`、`poll()`、`ioctl` |
| 等待数据 | 轮询 `ClearCommError` | `poll(POLLIN)` |
| 写不限时 | 0 与负数都是 | 只有负数 |
| 1.5 位停止位 | 支持 | `applySettings()` 拒绝 |
| mark / space 校验 | 支持 | 仅 Linux（`CMSPAR`） |
| `sendBreak` 时长 | 生效 | 忽略 |
| 端口枚举 | 注册表 | 目录扫描 |

## 已知限制

- **POSIX 分支还没有编译过，也没有在真实串口上跑过。** 代码是写完了，但这个模块是在
  Windows 上开发和构建的；在真正运行之前，请把 `termios` 那边的行为当作未验证。
- `waitForReadyRead()` 是轮询，等待期间每 2 毫秒醒一次。
- 没有“强制把字节推上线”的 flush；`waitForBytesWritten()` 等的是驱动队列清空。
- 没有暴露调制解调器事件通知（`WaitCommEvent`）；这里不自己开线程，和 `pipe` / `process`
  的形状一致。
