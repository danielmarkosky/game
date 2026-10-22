#pragma once

#include <SFML/Graphics.hpp>
#include <string>

// A tiny 3x5 pixel font, so the HUD doesn't need a font file.
// Supports A-Z, digits and : . , ; - ! ? ' " / ( ) < > + = % and space. Lowercase 'x' is a small cross,
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
    // The HUD font only has capitals (its lowercase 'x' is a cross), so titles are shown in them.
    std::string upper(std::string text);

    // The colours of the HUD and menus.
    inline const sf::Color WHITE(235, 235, 245);
    inline const sf::Color GOLD(255, 210, 60);
    inline const sf::Color RED(255, 90, 90);
    inline const sf::Color GREY(150, 150, 170);
    inline const sf::Color CYAN(110, 220, 240);
    sf::Color withAlpha(sf::Color color, sf::Uint8 alpha);

    void fillRect(sf::RenderTarget& target, sf::FloatRect area, sf::Color color);
    // A 1 px border just outside `area`.
    void frame(sf::RenderTarget& target, sf::FloatRect area, sf::Color color);
    // Darkens the whole screen.
    void dim(sf::RenderTarget& target, sf::Uint8 alpha);
}
