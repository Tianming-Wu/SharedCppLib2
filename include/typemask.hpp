#pragma once

/*
    Type mask for some type definitions, and make them compatible with WinAPI types,
    while also not forcing users to include Windows headers if they don't want to.
*/

#ifndef SCL_TYPEMASK_HPP
#define SCL_TYPEMASK_HPP

#ifndef SCL_NO_TYPEMASK

namespace scl2 {

typedef unsigned char byte_t, *pbyte_t;
typedef unsigned short word_t, *pword_t;
typedef unsigned long dword_t, *pdword_t;
typedef unsigned long long qword_t, *pqword_t;

typedef void* ptr_t;

#if defined(_WIN32) || defined(_WIN64)
    // Window handles are actually pointers to a struct { int unused; }.
    // But directly defining them using the same way may still cause convertion conflict.
    
    // *Stupid Microsoft: MSVC has a bug that if the struct is unnamed, then the export function will
    // not be correctly generated, so we have to name the struct.
    typedef struct __winhandle { int unused; } * winhandle_t;

    template<typename H>
    requires (sizeof(H) == sizeof(winhandle_t))
    inline winhandle_t from_handle(H handle) {
        return reinterpret_cast<winhandle_t>(reinterpret_cast<uintptr_t>(handle));
    }

    template<typename H>
    requires (sizeof(H) == sizeof(winhandle_t))
    inline H to_handle(winhandle_t handle) {
        return static_cast<H>(reinterpret_cast<uintptr_t>(handle));
    }

#endif

} // namespace scl2

#endif // SCL_NO_TYPEMASK
#endif // SCL_TYPEMASK_HPP