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


#ifndef SHAREDCPPLIB2_NO_CLASSHELPERS
    // This part provides some helper macros for class definitions

    // The enable_copy_move such macros are in the basics.hpp
    // and will be moved here in a future version.
    
    #define default_constructor(CLASSNAME) CLASSNAME() = default;
    #define default_destructor(CLASSNAME) ~CLASSNAME() = default;
    #define default_constructor_destructor(CLASSNAME) default_constructor(CLASSNAME) default_destructor(CLASSNAME)

    // class member access helpers
    #define cl_getter(TYPE, MEMBER) \
        inline TYPE get_##MEMBER() const { return MEMBER; }

    #define cl_setter(TYPE, MEMBER) \
        inline void set_##MEMBER(const TYPE& value) { MEMBER = value; }

    // This one also returns the old value on set, which saves a line if you need it.
    #define cl_olsetter(TYPE, MEMBER) \
        inline TYPE set_##MEMBER(const TYPE& value) { TYPE old_value = MEMBER; MEMBER = value; return old_value; }

    #define cl_getter_setter(TYPE, MEMBER) \
        cl_getter(TYPE, MEMBER) \
        cl_setter(TYPE, MEMBER)

    
    // class member access helpers, but works better.
    // requires you to define your class members using the `m_` prefix.

    #define cl_m_getter(MEMBER) \
        inline auto MEMBER() const { return m_##MEMBER; }

    #define cl_m_setter(MEMBER) \
        inline void set##MEMBER(const auto& value) { m_##MEMBER = value; }

    #define cl_m_olsetter(MEMBER) \
        inline auto set##MEMBER(const auto& value) { auto old_value = m_##MEMBER; m_##MEMBER = value; return old_value; }

    #define cl_m_getter_setter(MEMBER) \
        cl_m_getter(MEMBER) \
        cl_m_setter(MEMBER)

#endif // SHAREDCPPLIB2_NO_CLASSHELPERS



#endif // SHAREDCPPLIB2_NO_MACROS