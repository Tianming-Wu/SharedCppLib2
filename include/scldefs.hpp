/*
    Defined enum values for SharedCppLib2.

    Note that for most of the enums, I specifically did not use enum class.
    This will make it easier to use in most cases, but may result in some name conflicts.
    However, under the protection of the namespace, it should be fine. Other modules in Scl2
    almost always use the enum class to force a namespace prefix.
*/

#pragma once

#include "enum.hpp"

namespace scl2 {

/*
    Filesystem segment
*/

enum FileWriteOptions : uint8_t {
    FW_Null = 0,
    Append = 1,
    NeverOverride = 2,
    AlwaysOverride = 3,
};


/*
    UI related segment
*/

/*
    5 of 8 bits used.
    May change in the future version to be more compact and efficient.
    The current implementation wastes some bits, and can have self-conflicting values.
*/
enum Alignment : uint8_t {
    None = 0,

    Left = 1,
    Right = 1 << 1,

    Top = 1 << 2,
    Bottom = 1 << 3,

    TopLeft = Top | Left,
    TopRight = Top | Right,
    BottomLeft = Bottom | Left,
    BottomRight = Bottom | Right,

    HCenter = 1 << 4,
    VCenter = 1 << 5,

    Center = HCenter | VCenter
};
scl2_enum_bitopex(Alignment)

/*
    - Fill    : stretch to fill the region exactly (distorts aspect ratio)
    - Cover   : scale to cover the region, keeping aspect ratio (crops overflow)
    - Contain : scale to fit inside the region, keeping aspect ratio (letterboxes)
    - Center  : keep the original size, centered in the region
    - Tile    : repeat the original image in a grid
*/
enum class Stretch : uint8_t {
    Fill = 0,
    Cover = 1,
    Contain = 2,
    Center = 3,
    Tile = 4,
};

} // namespace scl2