# constant_time_compare - 常量时间比较指南

+ 名称: constant_time_compare  
+ 命名空间: n/a  
+ 文档版本: `1.0.0`

## 为什么需要它

在校验 HMAC 标签时，如果直接使用普通相等比较，可能泄露时间信息。
攻击者有机会根据耗时差异推断前缀是否匹配。
常量时间比较可以避免“遇到第一个不相等就提前退出”的行为，从而降低侧信道风险。

## 推荐实现模式

```cpp
#include <SharedCppLib2/bytearray.hpp>

bool constant_time_equal(const std::bytearray& a, const std::bytearray& b)
{
    // 长度差也纳入 diff，同时保持完整扫描路径
    const size_t max_len = (a.size() > b.size()) ? a.size() : b.size();
    unsigned int diff = static_cast<unsigned int>(a.size() ^ b.size());

    for (size_t i = 0; i < max_len; ++i)
    {
        const std::byte av = (i < a.size()) ? a[i] : B(0x00);
        const std::byte bv = (i < b.size()) ? b[i] : B(0x00);
        diff |= static_cast<unsigned int>(av ^ bv);
    }

    return diff == 0;
}
```

## HMAC 验签示例

```cpp
std::bytearray expected_tag = scl2::hmac<scl2::sha256>::compute(payload, key);
if (!constant_time_equal(expected_tag, received_tag)) {
    // reject
}
```

## 说明

- 在 C++ 中“常量时间”通常是源码层面的尽力保证，编译器优化仍可能影响最终行为。
- 但用于认证标签校验时，它通常明显优于直接使用 `==`。
- 在计算/验证标签前，请确保签名输入的二进制拼接规则稳定且可重建。
