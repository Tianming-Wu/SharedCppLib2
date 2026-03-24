/*
    Macros for the SharedCppLib2 project.

    This is a header-only module that provides various utility macros
    that can help you to arrange your code better.

*/

#pragma once

#include <functional>

// This macro disables all macros in this header.
#ifndef SHAREDCPPLIB2_NO_MACROS



#ifndef SHAEREDCPPLIB2_NO_CLASSMARKER
    // This part allows to to more explicitly mark the region of class

    #define public_member public
    #define public_method public
    #define public_constructor public

    #define protected_member protected
    #define protected_method protected
    #define protected_constructor protected

    #define private_member private
    #define private_method private
    #define private_constructor private

    #ifndef SHAREDCPPLIB2_NO_INTERFACE
        #define interface public
    #endif

#endif // SHAEREDCPPLIB2_NO_CLASSMARKER

#ifndef SHAREDCPPLIB2_NO_COPYMOVE
    // This part was in basics.hpp, and will be moved to here in a future version.

#endif // SHAREDCPPLIB2_NO_COPYMOVE

#ifndef SHAREDCPPLIB2_NO_STATIC_HELPER
    #define static_access(ACCESSOR, NAME, IMPL) \
        template <typename... Args> \
        inline static decltype(auto) NAME(Args&&... args) { \
            return std::invoke(IMPL, ACCESSOR(), std::forward<Args>(args)...); \
        }

#endif // SHAREDCPPLIB2_NO_STATIC_HELPER



#endif // SHAREDCPPLIB2_NO_MACROS