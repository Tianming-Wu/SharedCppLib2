/*
    Shared structure types for SharedCppLib2.

    Note:
        For type Rect, there're two macros named Rect_unwrap(rect) and Rect_unwrap_plain(rect).
        The former one unwraps to left, top, right, bottom, which is required by some WinAPI functions.
        The latter one unwraps to x, y, w, h, which is more intuitive and is fine for most use cases.
        
        These are present because it is not possible to easily convert a structure to plain form.
        They can at least makes your code looks tidier.
    
    Note 2:
        The types with a F suffix are the floating-point versions of the types without F. They
        are used to prevent precision loss in some cases where you need those scaling things.

    Note 3:
        All these types supports scaling. You can scale them by calling scale() or unscale them
        by calling unscale(), which is just a reverse action of scale. But be aware that there might
        be a precision loss when you scale and unscale. You can use F version to prevent that.
        
    Note 4:
        Structural binding support is provided for all these types, so you can use
        something like `auto [x, y] = GetSomePoint();` to take out values.

        For Point, the sequence is [x, y].
        For Size, the sequence is [w, h].
        For Rect, the sequence is [x, y, w, h].

    Note 5:
        These types are designed to be compatible with the corresponding types in WinAPI,
        so they can be easily converted back and forth. But they are not dependent on WinAPI,
        so you can use them in any platform.

        If you can not see the WinAPI support available, you need to check your include file.
        You'de better include platform.hpp instead of windows.h directly, since these file does
        rely on that file to recognize the platform and provide the support.

    Note 6:
        If you need something like shrinking a rect by a pixel, under scaling cases you might encounter
        visual problems, since there's a precision loss. And when the scaling is above 2, you will see a gap
        between the original and shrunk rect.

        I'm still working to solve this problem, and I already have some ideas. But currently, you can only
        use the F version, add a .4 or .5 to the value, and hope for the best.

    namespace:
        scl2
    link target:
        SharedCppLib2::basic
    types:
        scl2::Point, scl2::Size, scl2::Rect
        scl2::PointF, scl2::SizeF, scl2::RectF
*/

#pragma once

#include "scalable.hpp"
#include "structural_binding.hpp"

namespace scl2 {

struct Point
{
    int x, y;

    Point();
    Point(int x, int y);

    Point move(int dx, int dy) const;

    SCALABLE(x, y)

#ifdef OS_WINDOWS
    Point(const POINT& pt);
    operator POINT() const;
#endif
};

struct Size
{
    int w, h;

    Size();
    Size(int w, int h);

    Size expand(int dw, int dh) const;
    Size expand(int d) const;
    Size shrink(int dw, int dh) const;
    Size shrink(int d) const;

    SCALABLE(w, h)

#ifdef OS_WINDOWS
    Size(const SIZE& sz);
    operator SIZE() const;
#endif
};

struct Rect
{
    int x, y, w, h;

    Rect();
    Rect(int x, int y, int w, int h);
    Rect(Point pos, Size size);

    Rect expand(int dw, int dh) const;
    Rect expand(int d) const;
    Rect shrink(int dw, int dh) const;
    Rect shrink(int d) const;

#ifdef OS_WINDOWS
    Rect(const RECT& rc);
#endif

    Point position() const;
    Size size() const;

    // For some WinAPI that requires left, top, right, bottom, we can use this macro to unwrap the rect.
    #define Rect_unwrap(rect) rect.x, rect.y, rect.x + rect.w, rect.y + rect.h
    #define Rect_unwrap_plain(rect) rect.x, rect.y, rect.w, rect.h

    SCALABLE(x, y, w, h)
};

using Geometry = Rect;



// Floating point version,
// To prevent precision loss when converting back and forth between physical and logical geometry.

struct PointF
{
    double x, y;

    PointF();
    PointF(double x, double y);

    SCALABLE(x, y)

#ifdef OS_WINDOWS
    PointF(const POINT& pt);
    operator POINT() const;
#endif
};

struct SizeF
{
    double w, h;

    SizeF();
    SizeF(double w, double h);

    SCALABLE(w, h)

#ifdef OS_WINDOWS
    SizeF(const SIZE& sz);
    operator SIZE() const;
#endif
};

struct RectF
{
    double x, y, w, h;

    RectF();
    RectF(double x, double y, double w, double h);
    RectF(PointF pos, SizeF size);

#ifdef OS_WINDOWS
    RectF(const RECT& rc);
#endif

    PointF position() const;
    SizeF size() const;

    // For some WinAPI that requires left, top, right, bottom, we can use this macro to unwrap the rect.
    #define Rect_unwrap(rect) rect.x, rect.y, rect.x + rect.w, rect.y + rect.h
    #define Rect_unwrap_plain(rect) rect.x, rect.y, rect.w, rect.h

    SCALABLE(x, y, w, h)
};

using GeometryF = RectF;



} // namespace scl2

// Structural binding support for the above types.
// Allow users to take out values by `auto [x, y] = GetSomePoint();` etc.

STRUCTURAL_BINDING_BEGIN

STRUCTURAL_BINDING_DEFINE_SIZE(scl2::Point, 2)
STRUCTURAL_BINDING_DEFINE_SIZE(scl2::Size, 2)
STRUCTURAL_BINDING_DEFINE_SIZE(scl2::Rect, 4)

STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::Point, 0, int)
STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::Point, 1, int)

STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::Size, 0, int)
STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::Size, 1, int)

STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::Rect, 0, int)
STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::Rect, 1, int)
STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::Rect, 2, int)
STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::Rect, 3, int)


STRUCTURAL_BINDING_DEFINE_SIZE(scl2::PointF, 2)
STRUCTURAL_BINDING_DEFINE_SIZE(scl2::SizeF, 2)
STRUCTURAL_BINDING_DEFINE_SIZE(scl2::RectF, 4)

STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::PointF, 0, double)
STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::PointF, 1, double)

STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::SizeF, 0, double)
STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::SizeF, 1, double)

STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::RectF, 0, double)
STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::RectF, 1, double)
STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::RectF, 2, double)
STRUCTURAL_BINDING_DEFINE_ELEMENT(scl2::RectF, 3, double)

STRUCTURAL_BINDING_END

STRUCTURAL_BINDING_DEFINE_GET(scl2::Point, 0, x)
STRUCTURAL_BINDING_DEFINE_GET(scl2::Point, 1, y)

STRUCTURAL_BINDING_DEFINE_GET(scl2::Size, 0, w)
STRUCTURAL_BINDING_DEFINE_GET(scl2::Size, 1, h)

STRUCTURAL_BINDING_DEFINE_GET(scl2::Rect, 0, x)
STRUCTURAL_BINDING_DEFINE_GET(scl2::Rect, 1, y)
STRUCTURAL_BINDING_DEFINE_GET(scl2::Rect, 2, w)
STRUCTURAL_BINDING_DEFINE_GET(scl2::Rect, 3, h)

STRUCTURAL_BINDING_DEFINE_GET(scl2::PointF, 0, x)
STRUCTURAL_BINDING_DEFINE_GET(scl2::PointF, 1, y)

STRUCTURAL_BINDING_DEFINE_GET(scl2::SizeF, 0, w)
STRUCTURAL_BINDING_DEFINE_GET(scl2::SizeF, 1, h)

STRUCTURAL_BINDING_DEFINE_GET(scl2::RectF, 0, x)
STRUCTURAL_BINDING_DEFINE_GET(scl2::RectF, 1, y)
STRUCTURAL_BINDING_DEFINE_GET(scl2::RectF, 2, w)
STRUCTURAL_BINDING_DEFINE_GET(scl2::RectF, 3, h)
