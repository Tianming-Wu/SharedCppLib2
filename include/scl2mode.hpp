/*
    Which variant of the library a translation unit is compiled against.

    The same sources are built twice: the plain library, and the variant used by
    SCL2EventSubsystem (configured with -DSCL2_EVENT_SUBSYSTEM=ON). Modules that
    adapt themselves to the subsystem include this header.

    Nothing from the two variants may be linked together - the adapted modules do
    not even keep the same layout - so the variants carry a marker that makes the
    linker refuse the mix instead of letting it corrupt data at run time.
    SCL2_LIBRARY_VARIANT is "plain" or "event".
*/

#pragma once

#ifdef SCL2_EVENT_SUBSYSTEM
    #define SCL2_LIBRARY_VARIANT "event"
#else
    #define SCL2_LIBRARY_VARIANT "plain"
#endif

// MSVC reports the mismatch as LNK2038, naming the two values.
#if defined(_MSC_VER)

#pragma detect_mismatch("scl2_variant", SCL2_LIBRARY_VARIANT)

// Nothing to define: the pragma is the whole guard.
#define SCL2_MODE_DEFINE

#else

// A marker whose name depends on the variant. Every adapted header refers to the
// one it was compiled with, and SCL2_MODE_DEFINE in one source file of the module
// defines that same one, so a mix ends as an undefined symbol at link time.
//
// Only adapted headers may cause the reference (see pipe.hpp): a header-only
// module must not start asking for a library symbol.
namespace scl2::detail {

#ifdef SCL2_EVENT_SUBSYSTEM
extern "C" int scl2_variant_event;
[[gnu::used]] inline const int* const variant_marker = &scl2_variant_event;
#else
extern "C" int scl2_variant_plain;
[[gnu::used]] inline const int* const variant_marker = &scl2_variant_plain;
#endif

}

#ifdef SCL2_EVENT_SUBSYSTEM
    #define SCL2_MODE_DEFINE extern "C" int scl2_variant_event = 1;
#else
    #define SCL2_MODE_DEFINE extern "C" int scl2_variant_plain = 1;
#endif

#endif

namespace scl2 {

/// @brief Which variant this translation unit was compiled against.
inline constexpr const char* library_variant = SCL2_LIBRARY_VARIANT;

}
