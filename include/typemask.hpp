#pragma once

/*
    Type mask for some type definitions, and make them compatible with WinAPI types,
    while also not forcing users to include Windows headers if they don't want to.
*/

#ifndef SCL_TYPEMASK_HPP
#define SCL_TYPEMASK_HPP

#ifndef SCL_NO_TYPEMASK

typedef unsigned char byte_t, *pbyte_t;
typedef unsigned short word_t, *pword_t;
typedef unsigned long dword_t, *pdword_t;
typedef unsigned long long qword_t, *pqword_t;

typedef void* ptr_t;

#endif // SCL_NO_TYPEMASK
#endif // SCL_TYPEMASK_HPP