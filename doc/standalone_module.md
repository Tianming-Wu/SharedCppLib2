# Standalone Module

There are certain modules that can be used as "standalone module".

This means that they can work independently without other modules in this library, or at least is possible to do so (like by disabling certain features).

In this case, it is possible to copy the source files of the module into your project, without installing the whole SharedCppLib2 library.

Take json as an example. It is a standalone module, since it's SharedCppLib2 intergration is only enabled when the macro `SCL2_JSON_ENABLE_EXTENSIONS` is defined.

In this case, the module contain code that depends on other files in SharedCppLib2. By not defining it, the module can be used independently, and only depends on the standard library.

## What modules are standalone

Modules that are designed to be standalone has a `[SCL_STANDALONE_MODULE]` section in the beginning comment.

```cpp
/*
    ...

    [SCL_STANDALONE_MODULE]
    version: 1.0.0
    cpp_generation: cxx17 - cxx23
    standalone_dependency: 
*/
```

It may contain following sections (\* means it always exists):
- `version`\*: The version of the module. It is synced between source and header, and is strictly different between any modification (published).
- `cpp_generation`: The C++ standard that the module is compatible with. It may be a range, like `cxx17 - cxx23`, or a single standard, like `cxx20`. Check [About Compatibility](#about-compatibility).
- `standalone_dependency`: The module may depend on other standalone modules. If it does, it will be listed here. The module is not designed to work without the dependency listed here.


## About compatibility

Standalone modules are typically designed based on different standards with the whole SharedCppLib2.

SharedCppLib2 is focused on C++23, and some modules may use features that are only available in C++23. However, some modules are designed to be compatible with older standards, like C++17 or C++20.

It might be originally compatible with the standard, or by disabling certain part of the code.

Take json as an example, it used C++23's `generator` feature. Part of the code will be automatically disabled if you are using C++17 or C++20. You can still use `as_array()` and `as_object()` for iteration.

The module compatibility is described in `[SCL_STANDALONE_MODULE]` section, in the `cpp_generation` field. It may be a range, like `cxx17 - cxx23`, or a single standard, like `cxx20`. Module may work outside of described range, but it is not guaranteed. (It should always work with newest standard, since the C++ standard is backward compatible.)

If not listed, by default is cxx23.