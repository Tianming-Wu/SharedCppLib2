#include "scltypes.hpp"

namespace scl2 {

Point::Point() : x(0), y(0) {}
Point::Point(int x, int y) : x(x), y(y) {}

Point Point::move(int dx, int dy) const { return Point(x + dx, y + dy); }

#ifdef OS_WINDOWS
Point::Point(const POINT& pt) : x(pt.x), y(pt.y) {}
Point::operator POINT() const { return POINT{ x, y }; }
#endif



Size::Size() : w(0), h(0) {}
Size::Size(int w, int h) : w(w), h(h) {}

Size Size::expand(int dw, int dh) const { return Size(w + dw, h + dh); }
Size Size::expand(int d) const { return Size(w + d, h + d); }
Size Size::shrink(int dw, int dh) const { return Size(w - dw, h - dh); }
Size Size::shrink(int d) const { return Size(w - d, h - d); }

#ifdef OS_WINDOWS
Size::Size(const SIZE& sz) : w(sz.cx), h(sz.cy) {}
Size::operator SIZE() const { return SIZE{ w, h }; }
#endif



Rect::Rect() : x(0), y(0), w(0), h(0) {}
Rect::Rect(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}
Rect::Rect(Point pos, Size size) : x(pos.x), y(pos.y), w(size.w), h(size.h) {}

Rect Rect::expand(int dw, int dh) const { return Rect(x, y, w + dw, h + dh); }
Rect Rect::expand(int d) const { return Rect(x - d, y - d, w + 2 * d, h + 2 * d); }
Rect Rect::shrink(int dw, int dh) const { return Rect(x + dw, y + dh, w - 2 * dw, h - 2 * dh); }
Rect Rect::shrink(int d) const { return Rect(x + d, y + d, w - 2 * d, h - 2 * d); }

#ifdef OS_WINDOWS
Rect::Rect(const RECT& rc) : x(rc.left), y(rc.top), w(rc.right - rc.left), h(rc.bottom - rc.top) {}
#endif

Point Rect::position() const { return Point(x, y); }
Size Rect::size() const { return Size(w, h); }


// Floating point version

PointF::PointF() : x(0), y(0) {}
PointF::PointF(double x, double y) : x(x), y(y) {}

#ifdef OS_WINDOWS
PointF::PointF(const POINT& pt) : x(pt.x), y(pt.y) {}
PointF::operator POINT() const { return POINT{ (int)x, (int)y }; }
#endif


SizeF::SizeF() : w(0), h(0) {}
SizeF::SizeF(double w, double h) : w(w), h(h) {}

#ifdef OS_WINDOWS
SizeF::SizeF(const SIZE& sz) : w(sz.cx), h(sz.cy) {}
SizeF::operator SIZE() const { return SIZE{ (int)w, (int)h }; }
#endif



RectF::RectF() : x(0), y(0), w(0), h(0) {}
RectF::RectF(double x, double y, double w, double h) : x(x), y(y), w(w), h(h) {}
RectF::RectF(PointF pos, SizeF size) : x(pos.x), y(pos.y), w(size.w), h(size.h) {}

#ifdef OS_WINDOWS
RectF::RectF(const RECT& rc) : x(rc.left), y(rc.top), w(rc.right - rc.left), h(rc.bottom - rc.top) {}
#endif

PointF RectF::position() const { return PointF(x, y); }
SizeF RectF::size() const { return SizeF(w, h); }

} // namespace scl2