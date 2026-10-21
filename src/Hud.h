#pragma once

#include <SFML/Graphics.hpp>
#include <string>

// A tiny 3x5 pixel font, so the HUD doesn't need a font file.
// Supports A-Z, digits and : . - ! ? ' / ( ) < > and space. Lowercase 'x' is a small cross,
// used as the death counter's icon; other lowercase letters are drawn as capitals.
namespace hud
{
    constexpr float GLYPH_WIDTH = 3.f;
    constexpr float GLYPH_HEIGHT = 5.f;

    // Width in screen pixels of `text` drawn at `scale`.
    float textWidth(const std::string& text, float scale = 1.f);

    // Draws `text` with its top-left corner at `pos` (screen pixels), with a drop shadow.
    void drawText(sf::RenderTarget& target, const std::string& text, sf::Vector2f pos, sf::Color color, float scale = 1.f);
    // Same, horizontally centred on `centreX`.
    void drawCentred(sf::RenderTarget& target, const std::string& text, float centreX, float y, sf::Color color, float scale = 1.f);

    // "m:ss.t"
    std::string formatTime(float seconds);
}
