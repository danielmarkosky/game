#include "Hud.h"
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace
{
    using Glyph = std::array<const char*, 5>;

    Glyph glyphFor(char c)
    {
        if (c != 'x') c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        switch (c) {
            case '0': return {"###", "#.#", "#.#", "#.#", "###"};
            case '1': return {".#.", "##.", ".#.", ".#.", "###"};
            case '2': return {"###", "..#", "###", "#..", "###"};
            case '3': return {"###", "..#", "###", "..#", "###"};
            case '4': return {"#.#", "#.#", "###", "..#", "..#"};
            case '5': return {"###", "#..", "###", "..#", "###"};
            case '6': return {"###", "#..", "###", "#.#", "###"};
            case '7': return {"###", "..#", "..#", "..#", "..#"};
            case '8': return {"###", "#.#", "###", "#.#", "###"};
            case '9': return {"###", "#.#", "###", "..#", "###"};
            case 'A': return {".#.", "#.#", "###", "#.#", "#.#"};
            case 'B': return {"##.", "#.#", "##.", "#.#", "##."};
            case 'C': return {".##", "#..", "#..", "#..", ".##"};
            case 'D': return {"##.", "#.#", "#.#", "#.#", "##."};
            case 'E': return {"###", "#..", "##.", "#..", "###"};
            case 'F': return {"###", "#..", "##.", "#..", "#.."};
            case 'G': return {".##", "#..", "#.#", "#.#", ".##"};
            case 'H': return {"#.#", "#.#", "###", "#.#", "#.#"};
            case 'I': return {"###", ".#.", ".#.", ".#.", "###"};
            case 'J': return {"..#", "..#", "..#", "#.#", ".#."};
            case 'K': return {"#.#", "#.#", "##.", "#.#", "#.#"};
            case 'L': return {"#..", "#..", "#..", "#..", "###"};
            case 'M': return {"#.#", "###", "###", "#.#", "#.#"};
            case 'N': return {"##.", "#.#", "#.#", "#.#", "#.#"};
            case 'O': return {".#.", "#.#", "#.#", "#.#", ".#."};
            case 'P': return {"##.", "#.#", "##.", "#..", "#.."};
            case 'Q': return {".#.", "#.#", "#.#", "##.", ".##"};
            case 'R': return {"##.", "#.#", "##.", "#.#", "#.#"};
            case 'S': return {".##", "#..", ".#.", "..#", "##."};
            case 'T': return {"###", ".#.", ".#.", ".#.", ".#."};
            case 'U': return {"#.#", "#.#", "#.#", "#.#", "###"};
            case 'V': return {"#.#", "#.#", "#.#", ".#.", ".#."};
            case 'W': return {"#.#", "#.#", "###", "###", "#.#"};
            case 'X': return {"#.#", "#.#", ".#.", "#.#", "#.#"};
            case 'Y': return {"#.#", "#.#", ".#.", ".#.", ".#."};
            case 'Z': return {"###", "..#", ".#.", "#..", "###"};
            case ':': return {"...", ".#.", "...", ".#.", "..."};
            case '.': return {"...", "...", "...", "...", ".#."};
            case ',': return {"...", "...", "...", ".#.", "#.."};
            case ';': return {"...", ".#.", "...", ".#.", "#.."};
            case '"': return {"#.#", "#.#", "...", "...", "..."};
            case '+': return {"...", ".#.", "###", ".#.", "..."};
            case '=': return {"...", "###", "...", "###", "..."};
            case '%': return {"#.#", "..#", ".#.", "#..", "#.#"};
            case '-': return {"...", "...", "###", "...", "..."};
            case '!': return {".#.", ".#.", ".#.", "...", ".#."};
            case '?': return {"##.", "..#", ".#.", "...", ".#."};
            case '\'': return {".#.", ".#.", "...", "...", "..."};
            case '/': return {"..#", "..#", ".#.", "#..", "#.."};
            case '(': return {".#.", "#..", "#..", "#..", ".#."};
            case ')': return {".#.", "..#", "..#", "..#", ".#."};
            case '<': return {"..#", ".#.", "#..", ".#.", "..#"};
            case '>': return {"#..", ".#.", "..#", ".#.", "#.."};
            case 'x': return {"...", "#.#", ".#.", "#.#", "..."};
            default:  return {"...", "...", "...", "...", "..."};
        }
    }

    void drawGlyphs(sf::RenderTarget& target, const std::string& text, sf::Vector2f pos, sf::Color color, float scale)
    {
        sf::RectangleShape pixel({scale, scale});
        pixel.setFillColor(color);
        float x = std::floor(pos.x);
        for (char c : text) {
            Glyph glyph = glyphFor(c);
            for (int row = 0; row < 5; ++row) {
                for (int col = 0; col < 3; ++col) {
                    if (glyph[row][col] != '#') continue;
                    pixel.setPosition(x + col * scale, std::floor(pos.y) + row * scale);
                    target.draw(pixel);
                }
            }
            x += (hud::GLYPH_WIDTH + 1.f) * scale;
        }
    }
}

namespace hud
{
    float textWidth(const std::string& text, float scale)
    {
        if (text.empty()) return 0.f;
        return (text.size() * (GLYPH_WIDTH + 1.f) - 1.f) * scale;
    }

    void drawText(sf::RenderTarget& target, const std::string& text, sf::Vector2f pos, sf::Color color, float scale)
    {
        sf::Color shadow(0, 0, 0, static_cast<sf::Uint8>(160 * color.a / 255));
        float offset = std::max(1.f, std::floor(scale / 2.f));
        drawGlyphs(target, text, pos + sf::Vector2f(offset, offset), shadow, scale);
        drawGlyphs(target, text, pos, color, scale);
    }

    void drawCentred(sf::RenderTarget& target, const std::string& text, float centreX, float y, sf::Color color, float scale)
    {
        drawText(target, text, {std::floor(centreX - textWidth(text, scale) * 0.5f), y}, color, scale);
    }

    std::string formatTime(float seconds)
    {
        int tenths = static_cast<int>(seconds * 10.f);
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%d:%02d.%d", tenths / 600, (tenths / 10) % 60, tenths % 10);
        return buffer;
    }

    std::string upper(std::string text)
    {
        for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return text;
    }

    sf::Color withAlpha(sf::Color color, sf::Uint8 alpha)
    {
        color.a = alpha;
        return color;
    }

    void fillRect(sf::RenderTarget& target, sf::FloatRect area, sf::Color color)
    {
        sf::RectangleShape rect({area.width, area.height});
        rect.setPosition(area.left, area.top);
        rect.setFillColor(color);
        target.draw(rect);
    }

    void frame(sf::RenderTarget& target, sf::FloatRect a, sf::Color color)
    {
        fillRect(target, {a.left - 1.f, a.top - 1.f, a.width + 2.f, 1.f}, color);
        fillRect(target, {a.left - 1.f, a.top + a.height, a.width + 2.f, 1.f}, color);
        fillRect(target, {a.left - 1.f, a.top, 1.f, a.height}, color);
        fillRect(target, {a.left + a.width, a.top, 1.f, a.height}, color);
    }

    void dim(sf::RenderTarget& target, sf::Uint8 alpha)
    {
        const sf::Vector2f size(target.getView().getSize());
        fillRect(target, {0.f, 0.f, size.x, size.y}, sf::Color(0, 0, 0, alpha));
    }
}
