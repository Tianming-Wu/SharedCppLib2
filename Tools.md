# Some tools for SharedCpplib2

It has been hard for some cases with bytearray, so you may need this:

Find: `(?<=append\()(0x\d{2})(?=\))`

Replace: `std::byte{$1}`