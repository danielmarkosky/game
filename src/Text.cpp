#include "Text.h"
#include "Config.h"
#include <array>
#include <cmath>
#include <sstream>

namespace
{
    // As laid out by make_font() in tools/make_art.py.
    constexpr char FIRST = ' ', LAST = '~';
    constexpr int CELL_W = 6, CELL_H = 9, COLUMNS = 16;
    constexpr int SPACE_WIDTH = 3;

    struct Font
    {
        sf::Texture texture;
        std::array<int, LAST - FIRST + 1> widths{};
    };

    int indexOf(char c)
    {
        return (c < FIRST || c > LAST) ? '?' - FIRST : c - FIRST;
    }

    const Font& font()
    {
        static const Font loaded = [] {
            Font f;
            f.texture.loadFromFile(config::assetPath("gfx/font.png"));
            // Each glyph is as wide as its pixels reach.
            const sf::Image image = f.texture.copyToImage();
            for (int i = 0; i < static_cast<int>(f.widths.size()); ++i) {
                const unsigned x0 = (i % COLUMNS) * CELL_W, y0 = (i / COLUMNS) * CELL_H;
                int width = 0;
                for (int x = 0; x < CELL_W; ++x) {
                    for (int y = 0; y < CELL_H; ++y) {
                        if (x0 + x < image.getSize().x && y0 + y < image.getSize().y && image.getPixel(x0 + x, y0 + y).a > 0) width = x + 1;
                    }
                }
                f.widths[i] = width > 0 ? width : SPACE_WIDTH;
            }
            return f;
        }();
        return loaded;
    }

    void drawGlyphs(sf::RenderTarget& target, const std::string& s, sf::Vector2f pos, sf::Color color, size_t count)
    {
        const Font& f = font();
        sf::Sprite sprite(f.texture);
        sprite.setColor(color);
        float x = std::floor(pos.x);
        for (size_t i = 0; i < s.size() && i < count; ++i) {
            const int index = indexOf(s[i]);
            sprite.setTextureRect(sf::IntRect((index % COLUMNS) * CELL_W, (index / COLUMNS) * CELL_H, CELL_W, CELL_H));
            sprite.setPosition(x, std::floor(pos.y));
            target.draw(sprite);
            x += f.widths[index] + 1;
        }
    }
}

namespace text
{
    float width(const std::string& s)
    {
        float total = 0.f;
        for (char c : s) total += font().widths[indexOf(c)] + 1;
        return s.empty() ? 0.f : total - 1.f;
    }

    void draw(sf::RenderTarget& target, const std::string& s, sf::Vector2f pos, sf::Color color, size_t count)
    {
        drawGlyphs(target, s, pos + sf::Vector2f(1.f, 1.f), sf::Color(0, 0, 0, static_cast<sf::Uint8>(150 * color.a / 255)), count);
        drawGlyphs(target, s, pos, color, count);
    }

    std::vector<std::string> wrap(const std::string& s, float maxWidth)
    {
        std::vector<std::string> lines;
        std::istringstream words(s);
        std::string line;
        for (std::string word; words >> word;) {
            const std::string longer = line.empty() ? word : line + " " + word;
            if (!line.empty() && width(longer) > maxWidth) {
                lines.push_back(line);
                line = word;
            } else {
                line = longer;
            }
        }
        if (!line.empty()) lines.push_back(line);
        return lines;
    }
}
