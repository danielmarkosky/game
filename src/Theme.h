#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <memory>
#include <string>

// The look of a group of levels: the tiles and the background behind them. Made by
// tools/make_art.py into assets/gfx/themes/<name>/; assets/levels/levels.txt says which levels
// use which.
struct Theme
{
    static constexpr const char* DEFAULT = "cave";

    // Loads a theme, or shares the one already loaded while something still uses it.
    // Throws std::runtime_error if its files are missing.
    static std::shared_ptr<const Theme> get(const std::string& name);

    std::string name;
    sf::Texture tiles;
    // Background layers, back to front: 320x180 each, repeating sideways.
    std::array<sf::Texture, 3> layers; // sky, far, near
};
