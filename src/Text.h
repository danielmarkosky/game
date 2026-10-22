#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// The font conversations are written in: assets/gfx/font.png, made by tools/make_art.py.
// Printable ASCII in upper and lower case, each letter as wide as it needs; capitals are 7 px
// tall and g j p q y hang 2 px below the line. (The HUD and menus use the tiny capitals in Hud.h.)
namespace text
{
    constexpr float LINE_HEIGHT = 10.f;

    // Width in pixels of `s`.
    float width(const std::string& s);

    // Draws `s` with its top-left corner at `pos`, with a drop shadow. Only the first `count`
    // characters, for text that types itself out.
    void draw(sf::RenderTarget& target, const std::string& s, sf::Vector2f pos, sf::Color color,
              size_t count = std::string::npos);

    // Splits `s` at spaces into lines no wider than `maxWidth`. A word wider than that gets a
    // line of its own.
    std::vector<std::string> wrap(const std::string& s, float maxWidth);
}
