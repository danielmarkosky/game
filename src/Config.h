#pragma once

#include <string>

// Shared constants. Anything that more than one file needs to agree on lives here.
namespace config
{
    // Logical resolution; the window is this times WINDOW_SCALE.
    constexpr unsigned GAME_WIDTH = 320;
    constexpr unsigned GAME_HEIGHT = 180;
    constexpr unsigned WINDOW_SCALE = 4;

    constexpr float TILE_SIZE = 8.f;

    // World units are pixels and seconds, and world y points up. How the player moves (gravity
    // too) is in physics(), src/Physics.h.

    // Physics runs at a fixed rate, independent of the frame rate.
    constexpr float FIXED_DT = 1.f / 120.f;

    inline std::string assetPath(const std::string& relative)
    {
#ifdef ASSETS_DIR
        return std::string(ASSETS_DIR) + "/" + relative;
#else
        return "assets/" + relative;
#endif
    }
}
