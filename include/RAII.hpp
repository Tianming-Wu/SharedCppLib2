#pragma once

/*
    Generic RAII wrapper for any resource that needs to be release.

    Good for Windows HANDLEs.

    (unique_ptr: ?)

*/

#include <functional>
#include <type_traits>

namespace scl2 {

template <
    typename _Resource_Type,
    std::function<void(_Resource_Type&)> deleter
>
class RAII
{
public:
    using resource_type = _Resource_Type;
    // using deleter_type = std::function<void(resource_type&)>;

private:
    resource_type resource;
    
public:
    RAII(resource_type resource)
        : resource(std::move(resource)) {}

    ~RAII() {
        if (deleter) {
            deleter(resource);
        }
    }

    operator resource_type&() { return resource; }




};

#ifdef OS_WINDOWS

using RAII_HANDLE = RAII<HANDLE, std::function<void(HANDLE&)>>;

#endif


} // namespace scl2