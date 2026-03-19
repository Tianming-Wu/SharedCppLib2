# constant_time_compare - Constant-Time Comparison Guide

+ Name: constant_time_compare  
+ Namespace: n/a  
+ Document Version: `1.0.0`

## Why It Matters

When verifying HMAC tags, using normal equality checks may leak timing information.
Attackers can sometimes infer how many prefix bytes are correct.
A constant-time comparison avoids early-exit behavior and reduces this side-channel.

## Recommended Pattern

```cpp
#include <SharedCppLib2/bytearray.hpp>

bool constant_time_equal(const std::bytearray& a, const std::bytearray& b)
{
    // Include length mismatch in the final diff while still scanning data path.
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

## HMAC Verification Example

```cpp
std::bytearray expected_tag = scl2::hmac<scl2::sha256>::compute(payload, key);
if (!constant_time_equal(expected_tag, received_tag)) {
    // reject
}
```

## Notes

- Constant-time in C++ is best-effort at source level; compiler optimizations can affect behavior.
- Still better than direct `==` for authentication tag checks.
- Keep payload canonicalization deterministic before computing/verifying tags.
