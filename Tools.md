# Some tools for SharedCpplib2


### Bytearray
It has been hard for some cases with bytearray, so you may need this:

Find: `(?<=append\()(0x\d{2})(?=\))`

Replace: `std::byte{$1}`

> **Note**: This is for the cases where you have written some ba.append(0xXX). In most of the cases, that won't be your dedicated behavior, since 0xXX is an integer (4 bytes) instead of a single byte.
You can use that to quickly convert those cases to the correct form.
>
> Also, you can Use this replace too: `B($1)`, which is a macro that I have defined in the header file, and it will do the same thing as std::byte{$1}.
>
> This is ***not*** my fault, it's the feature of C++ interpretation, who consider any numbers as integers by default. So it just consider 0xXX the same as 0x000000XX. If I leave one function that only accepts std::byte, you will have to explicitly cast the integer to std::byte. If I accept only uint8_t, you will receive warnings about losing precision. And I need to support all number types. So, currently I was able to ensure that all values are passed in the correct type, but I can do nothing about 0xXX grammar.