#include "debug.hpp"

#ifndef OS_WINDOWS
    #include <syslog.h>
#endif

// If whatever something needs to be initialized
ods::ods()
{
#ifdef OS_WINDOWS

#else

#endif
}

ods::~ods()
{
    ss << "\n"; // Append a newline, so users don't have to add it.
#ifdef OS_WINDOWS
    OutputDebugStringA(ss.str().c_str());
#else
    syslog(LOG_DEBUG, "%s", ss.str().c_str());
#endif
}