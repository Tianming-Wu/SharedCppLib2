/*
    Drawer module for SharedCppLib2.

    Rasterization primitives (pixel / line / rectangle / circle) that render
    onto any draw_target<Pixel> surface (e.g. bitmap_1c, bitmap<scl2::color>,
    or any custom surface implementing draw_target<Pixel>).

    The circle algorithm is ported from the CBitmap project: instead of the
    classic integer midpoint algorithm, it evaluates each pixel center against
    an inner/outer radius band (radius +/- 0.5). This yields smoother, more
    accurate circles, especially for half-pixel centers and even sizes.

    Header-only. Include "drawer.hpp" and link against the bitmap target.
*/

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <type_traits>

#include "bitmap.hpp"
#include "color.hpp"

namespace scl2 {

/// How shapes are filled when drawn.
enum class fill_mode : uint8_t {
    none,       // outline only (rectangle border / circle ring)
    solid,      // filled shape
    alternate,  // reserved; not implemented (matches CBitmap)
};

/// Stroke (line / outline) settings for a drawer.
template <typename Pixel>
class pen {
public:
    Pixel color{};
    int width = 1;          // stroke width in pixels (>= 1)
    uint8_t alpha = 255;    // 0..255 opacity, used by blending / AA
    pen() = default;
    explicit pen(const Pixel& c) : color(c) {}
};

/// Fill settings for a drawer.
template <typename Pixel>
class brush {
public:
    Pixel color{};
    uint8_t alpha = 255;
    brush() = default;
    explicit brush(const Pixel& c) : color(c) {}
};

/// Rasterizes primitives onto a draw_target<Pixel> using a configurable pen.
template <typename Pixel>
class drawer {
public:
    /// @param target  The surface to draw on (must outlive the drawer).
    /// @param on_color  Pen color for drawn pixels (foreground).
    /// @param off_color Background color (reserved for future erase ops).
    drawer(draw_target<Pixel>& target, const Pixel& on_color, const Pixel& off_color)
        : m_target(&target), m_on(on_color), m_off(off_color),
          m_pen(on_color), m_brush(on_color) {}

    // ---- color configuration (legacy pen/brush shortcuts) ----
    void set_color(const Pixel& on, const Pixel& off) {
        m_on = on; m_off = off;
        m_pen.color = on; m_brush.color = on;
    }
    void set_on_color(const Pixel& on) { m_on = on; m_pen.color = on; }
    void set_off_color(const Pixel& off) { m_off = off; }
    const Pixel& on_color() const { return m_on; }
    const Pixel& off_color() const { return m_off; }

    // ---- pen / brush ----
    void set_pen(const pen<Pixel>& p) { m_pen = p; }
    void set_brush(const brush<Pixel>& b) { m_brush = b; }
    const pen<Pixel>& current_pen() const { return m_pen; }
    const brush<Pixel>& current_brush() const { return m_brush; }

    // ---- primitives ----
    void draw_pixel(int x, int y);
    void draw_line(int x0, int y0, int x1, int y1);
    void draw_rectangle(int x0, int y0, int x1, int y1, fill_mode fill = fill_mode::none);
    void draw_circle(double cx, double cy, double radius, fill_mode fill = fill_mode::none);

    // ---- anti-aliased primitives ----
    // 1-bit targets have no partial opacity, so these fall back to the
    // hard-edge versions (no thresholding that could break a line).
    /// @brief Anti-aliased line (Wu's algorithm).
    void draw_line_aa(int x0, int y0, int x1, int y1);
    /// @brief Anti-aliased circle (coverage from the floating-point band).
    void draw_circle_aa(double cx, double cy, double radius, fill_mode fill = fill_mode::none);

    /// @brief Write a pixel blended with the current content.
    /// @param alpha  0..255 opacity of `src` (255 = overwrite).
    /// Integral (grayscale) pixels get a linear blend; 1-bit targets threshold
    /// at >127; other pixel types (e.g. color) fall back to hard overwrite.
    void blend_at(int x, int y, const Pixel& src, uint8_t alpha);

protected:
    bool in_bounds(int x, int y) const {
        return x >= 0 && y >= 0
            && static_cast<size_t>(x) < m_target->width()
            && static_cast<size_t>(y) < m_target->height();
    }
    // Stroke plot: pen color, hard overwrite (1-bit safe).
    void plot(int x, int y) {
        if (in_bounds(x, y)) m_target->set_pixel(x, y, m_pen.color);
    }
    // Fill plot: brush color, hard overwrite.
    void fill_plot(int x, int y) {
        if (in_bounds(x, y)) m_target->set_pixel(x, y, m_brush.color);
    }

private:
    draw_target<Pixel>* m_target;
    Pixel m_on;
    Pixel m_off;
    pen<Pixel> m_pen;
    brush<Pixel> m_brush;
};

// ---- implementation (header-only) ----

template <typename Pixel>
void drawer<Pixel>::draw_pixel(int x, int y)
{
    plot(x, y);
}

template <typename Pixel>
void drawer<Pixel>::blend_at(int x, int y, const Pixel& src, uint8_t alpha)
{
    if (!in_bounds(x, y)) return;
    if constexpr (std::same_as<Pixel, bool>) {
        // 1-bit has no partial opacity — threshold like the hard-edge algorithms.
        if (alpha > 127) m_target->set_pixel(x, y, src);
    } else if constexpr (std::is_integral_v<Pixel>) {
        // Grayscale: dst = lerp(dst, src, alpha/255).
        const int d = static_cast<int>(m_target->get_pixel(x, y));
        const int s = static_cast<int>(src);
        const int v = (d * (255 - static_cast<int>(alpha)) + s * static_cast<int>(alpha)) / 255;
        m_target->set_pixel(x, y, static_cast<Pixel>(v));
    } else if constexpr (std::same_as<Pixel, rgba8>) {
        // Source-over alpha compositing (straight alpha).
        const rgba8 d = m_target->get_pixel(x, y);
        const rgba8 s = src;
        const int sa = static_cast<int>(s.a) * static_cast<int>(alpha) / 255;
        const int da = static_cast<int>(d.a);
        const int out_a = sa + da * (255 - sa) / 255;
        if (out_a == 0) { m_target->set_pixel(x, y, rgba8(0, 0, 0, 0)); return; }
        const int sr = static_cast<int>(s.r) * sa;
        const int sg = static_cast<int>(s.g) * sa;
        const int sb = static_cast<int>(s.b) * sa;
        const int dr = static_cast<int>(d.r) * da;
        const int dg = static_cast<int>(d.g) * da;
        const int db = static_cast<int>(d.b) * da;
        m_target->set_pixel(x, y, rgba8(
            static_cast<uint8_t>((sr + dr * (255 - sa) / 255) / out_a),
            static_cast<uint8_t>((sg + dg * (255 - sa) / 255) / out_a),
            static_cast<uint8_t>((sb + db * (255 - sa) / 255) / out_a),
            static_cast<uint8_t>(out_a)));
    } else {
        // Other pixel types (e.g. color): no blending yet — hard overwrite.
        if (alpha > 127) m_target->set_pixel(x, y, src);
    }
}

template <typename Pixel>
void drawer<Pixel>::draw_line(int x0, int y0, int x1, int y1)
{
    // Bresenham's line algorithm
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (true) {
        plot(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

template <typename Pixel>
void drawer<Pixel>::draw_line_aa(int x0, int y0, int x1, int y1)
{
    // Anti-aliased line (Wu's algorithm). 1-bit targets have no partial
    // opacity, so they keep the hard-edge Bresenham line.
    if constexpr (std::same_as<Pixel, bool>) {
        draw_line(x0, y0, x1, y1);
        return;
    }
    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    const Pixel& pc = m_pen.color;
    const int palpha = static_cast<int>(m_pen.alpha);

    if (dx == 0 && dy == 0) { blend_at(x0, y0, pc, 255); return; }

    // Endpoints at full pen alpha (Wu's endpoint handling).
    blend_at(x0, y0, pc, static_cast<uint8_t>(palpha));
    blend_at(x1, y1, pc, static_cast<uint8_t>(palpha));

    if (dx > dy) {
        // X-major: step along x.
        const double gradient = static_cast<double>(dy) / dx;
        double y = y0;
        for (int x = x0 + sx; x != x1; x += sx) {
            y += gradient * sx;
            const int yi = static_cast<int>(y);
            const double frac = y - yi;
            const int a_lo = static_cast<int>((1.0 - frac) * palpha);
            const int a_hi = static_cast<int>(frac * palpha);
            blend_at(x, yi, pc, static_cast<uint8_t>(a_lo));
            blend_at(x, yi + sy, pc, static_cast<uint8_t>(a_hi));
        }
    } else {
        // Y-major: step along y.
        const double gradient = static_cast<double>(dx) / dy;
        double x = x0;
        for (int y = y0 + sy; y != y1; y += sy) {
            x += gradient * sy;
            const int xi = static_cast<int>(x);
            const double frac = x - xi;
            const int a_lo = static_cast<int>((1.0 - frac) * palpha);
            const int a_hi = static_cast<int>(frac * palpha);
            blend_at(xi, y, pc, static_cast<uint8_t>(a_lo));
            blend_at(xi + sx, y, pc, static_cast<uint8_t>(a_hi));
        }
    }
}

template <typename Pixel>
void drawer<Pixel>::draw_rectangle(int x0, int y0, int x1, int y1, fill_mode fill)
{
    const int l = std::min(x0, x1);
    const int r = std::max(x0, x1);
    const int t = std::min(y0, y1);
    const int b = std::max(y0, y1);
    switch (fill) {
    case fill_mode::solid:
        for (int y = t; y <= b; ++y)
            for (int x = l; x <= r; ++x) fill_plot(x, y);
        break;
    case fill_mode::none:
    default:
        for (int x = l; x <= r; ++x) { plot(x, t); plot(x, b); }
        for (int y = t + 1; y < b; ++y) { plot(l, y); plot(r, y); }
        break;
    case fill_mode::alternate:
        break; // not implemented (matches CBitmap)
    }
}

template <typename Pixel>
void drawer<Pixel>::draw_circle(double cx, double cy, double radius, fill_mode fill)
{
    if (radius < 0) return;

    // Bounding box of the outer radius ring.
    const int min_x = std::max(0, static_cast<int>(std::floor(cx - radius - 0.5)));
    const int max_x = std::min(static_cast<int>(m_target->width()) - 1, static_cast<int>(std::ceil(cx + radius - 0.5)));
    const int min_y = std::max(0, static_cast<int>(std::floor(cy - radius - 0.5)));
    const int max_y = std::min(static_cast<int>(m_target->height()) - 1, static_cast<int>(std::ceil(cy + radius - 0.5)));

    // Ring band: pixel centers within [radius-0.5, radius+0.5] are on the ring.
    const double inner_r = std::max(0.0, radius - 0.5);
    const double outer_r = radius + 0.5;
    const double inner_r2 = inner_r * inner_r;
    const double outer_r2 = outer_r * outer_r;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const double dx = (x + 0.5) - cx; // distance from pixel center
            const double dy = (y + 0.5) - cy;
            const double d2 = dx * dx + dy * dy;

            if (fill == fill_mode::solid) {
                if (d2 <= outer_r2) fill_plot(x, y);
            } else {
                if (d2 >= inner_r2 && d2 <= outer_r2) plot(x, y); // ring
            }
        }
    }
}

template <typename Pixel>
void drawer<Pixel>::draw_circle_aa(double cx, double cy, double radius, fill_mode fill)
{
    // Coverage-based anti-aliased circle. 1-bit targets fall back to the
    // hard-edge floating-point circle.
    if constexpr (std::same_as<Pixel, bool>) {
        draw_circle(cx, cy, radius, fill);
        return;
    }
    if (radius < 0) return;

    const int min_x = std::max(0, static_cast<int>(std::floor(cx - radius - 0.5)));
    const int max_x = std::min(static_cast<int>(m_target->width()) - 1, static_cast<int>(std::ceil(cx + radius + 0.5)));
    const int min_y = std::max(0, static_cast<int>(std::floor(cy - radius - 0.5)));
    const int max_y = std::min(static_cast<int>(m_target->height()) - 1, static_cast<int>(std::ceil(cy + radius + 0.5)));

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const double dx = (x + 0.5) - cx;
            const double dy = (y + 0.5) - cy;
            const double d = std::sqrt(dx * dx + dy * dy);
            double coverage;
            uint8_t src_alpha;
            if (fill == fill_mode::solid) {
                // Full inside, fading at the edge.
                coverage = std::clamp(radius + 0.5 - d, 0.0, 1.0);
                src_alpha = m_brush.alpha;
            } else {
                // Ring: a 1px band centered on `radius`.
                coverage = std::clamp(0.5 - std::abs(d - radius), 0.0, 1.0);
                src_alpha = m_pen.alpha;
            }
            const int alpha = static_cast<int>(coverage * src_alpha);
            if (alpha <= 0) continue;
            const Pixel& pc = (fill == fill_mode::solid) ? m_brush.color : m_pen.color;
            blend_at(x, y, pc, static_cast<uint8_t>(alpha));
        }
    }
}

} // namespace scl2
