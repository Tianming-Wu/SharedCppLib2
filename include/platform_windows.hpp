/*
    Note: This is NOT a part of the platform module. This is a header-only module that
        provides some fixes for the windows.h.

    Microsoft is doing a bad job at managing their header files. And they have a bad habit
    of defining macros for everything, which causes a lot of problems. For example, they define
    a macro called "min" and "max", which causes problems when you want to use std::min and std::max.
    They also define a macro called "ERROR", which causes problems when you want to use the ERROR
    constant in Windows API.

    The goal of this header is to fix some of these problems. The file will be in recent update,
    that I add anything that would be useful in my own development.

*/

#pragma once

// ---- Preprocessor Macros ----
// These macros must be defined before including windows.h, otherwise they won't work.
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX // Exclude min and max macros


// ---- windows.h ----
// Our main character here.
#include <windows.h>




// ---- Process Related Macros ----

// Comment: these functions completely missed the A version, so they are inaccessible when UNICODE is defined.
#undef Process32First
#undef Process32Next
#undef PROCESSENTRY32


// ---- Event Related Macros ----

// This collided with my Eventloop (similar to Qt) CreateEvent widget event.
#undef CreateEvent
#undef OpenEvent


// ---- Error Code Macros ----
#undef ERROR


// Luckily, WIN USER and GDI are not that bad in naming.
// But seriously, now it would be just better to use enum, which is ALSO a thing in C standard.


// ---- Standard Access Types ----
// What the hell is WinNT developers thinking when they use these COMMON names for their own types?
#undef DELETE
#undef READ

// ---- Compiler Crash Report ----
// Uh oh...

// Just a joke.