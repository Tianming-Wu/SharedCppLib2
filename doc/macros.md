# Macros

///TODO: add document header



## Description




### Class helper macros
>[!WARNING]
> This part of the module is deprecated, since VSCode's engine break with these macros expanding
to class specifiers. Do not use them (or you can't since they are commented out).

This is for whoever thinks using public, protected and private is a little bit boring.

And it just feel mixed.

This part helps you better manage your class members.

```cpp

class my_class {
public_constructor:
    my_class() = default;

interface:
    void public_method();

private_members:
    int private_member;

factories:
    factory my_class build(int param);
    // Note that VSCode's code completion won't correctly regard factory as "static". You may need to manually remove the "factory" in the auto generated function implementaion.
    // Or, just use static, it's fine.

};

