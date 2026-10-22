#include "WorldMap.h"
#include "Theme.h"
#include <algorithm>
#include <climits>
#include <cmath>

namespace
{
    const sf::Color HAZARD(230, 70, 90);
    const sf::Color PLATFORM(200, 150, 90);
    const sf::Color HOOK(150, 165, 190);
    const sf::Color CHECKPOINT(126, 240, 142);

    // The average of the opaque pixels in part of an image.
    sf::Color average(const sf::Image& image, sf::IntRect area)
    {
        unsigned r = 0, g = 0, b = 0, n = 0;
        for (int y = area.top; y < area.top + area.height; ++y) {
            for (int x = area.left; x < area.left + area.width; ++x) {
                const sf::Color c = image.getPixel(static_cast<unsigned>(x), static_cast<unsigned>(y));
                if (c.a < 128) continue;
                r += c.r, g += c.g, b += c.b, ++n;
            }
        }
        if (n == 0) return sf::Color::Black;
        return sf::Color(static_cast<sf::Uint8>(r / n), static_cast<sf::Uint8>(g / n), static_cast<sf::Uint8>(b / n));
    }

    // Each pixel is the average of a `zoom` x `zoom` block, so thin corridors fade instead of vanishing.
    sf::Image shrink(const sf::Image& image, int zoom)
    {
        const sf::Vector2u size = image.getSize();
        const unsigned w = (size.x + zoom - 1) / zoom, h = (size.y + zoom - 1) / zoom;
        sf::Image out;
        out.create(w, h, sf::Color::Transparent);
        for (unsigned y = 0; y < h; ++y) {
            for (unsigned x = 0; x < w; ++x) {
                unsigned r = 0, g = 0, b = 0, a = 0, n = 0;
                for (unsigned dy = 0; dy < unsigned(zoom); ++dy) {
                    for (unsigned dx = 0; dx < unsigned(zoom); ++dx) {
                        const unsigned sx = x * zoom + dx, sy = y * zoom + dy;
                        if (sx >= size.x || sy >= size.y) continue;
                        const sf::Color c = image.getPixel(sx, sy);
                        r += c.r * c.a, g += c.g * c.a, b += c.b * c.a, a += c.a, ++n;
                    }
                }
                if (a == 0) continue;
                out.setPixel(x, y, sf::Color(static_cast<sf::Uint8>(r / a), static_cast<sf::Uint8>(g / a),
                                             static_cast<sf::Uint8>(b / a), static_cast<sf::Uint8>(a / n)));
            }
        }
        return out;
    }
}

WorldMap::WorldMap(std::vector<Room> roomList) : rooms(std::move(roomList))
{
    int left = INT_MAX, bottom = INT_MAX, right = INT_MIN, top = INT_MIN;
    for (const Room& room : rooms) {
        const sf::FloatRect b = room.level->bounds();
        const sf::FloatRect r(room.position.x + b.left / Level::TILE_SIZE, room.position.y + b.top / Level::TILE_SIZE,
                              b.width / Level::TILE_SIZE, b.height / Level::TILE_SIZE);
        rects.push_back(r);
        left = std::min(left, static_cast<int>(r.left));
        bottom = std::min(bottom, static_cast<int>(r.top));
        right = std::max(right, static_cast<int>(r.left + r.width));
        top = std::max(top, static_cast<int>(r.top + r.height));
    }
    world = rooms.empty() ? sf::IntRect() : sf::IntRect(left, bottom, right - left, top - bottom);
}

WorldMap::Colors WorldMap::themeColors(const Theme& theme)
{
    // Taken from the theme's own art: its rock, the grass (or snow, or embers) on top, and its sky.
    const sf::Image tiles = theme.tiles.copyToImage();
    const sf::Image sky = theme.layers[0].copyToImage();
    const sf::Color skyColor = average(sky, {0, 0, static_cast<int>(sky.getSize().x), static_cast<int>(sky.getSize().y)});
    auto darker = [](sf::Color c, float f) { return sf::Color(sf::Uint8(c.r * f), sf::Uint8(c.g * f), sf::Uint8(c.b * f)); };
    return {average(tiles, {0, 0, 8, 8}), average(tiles, {32, 0, 8, 2}), darker(skyColor, 0.6f)};
}

void WorldMap::reveal(const std::vector<bool>& newShown)
{
    if (newShown == shown && textures[0].getSize().x > 0) return;
    shown = newShown;

    sf::Image image;
    image.create(static_cast<unsigned>(std::max(1, world.width)), static_cast<unsigned>(std::max(1, world.height)), sf::Color::Transparent);
    for (size_t ii = 0; ii < rooms.size(); ++ii) {
        if (ii >= shown.size() || !shown[ii]) continue;
        const Level& level = *rooms[ii].level;
        const Colors colors = themeColors(level.theme());
        const sf::FloatRect b = level.bounds();
        const int x0 = static_cast<int>(b.left / Level::TILE_SIZE), y0 = static_cast<int>(b.top / Level::TILE_SIZE);
        const int x1 = x0 + static_cast<int>(b.width / Level::TILE_SIZE), y1 = y0 + static_cast<int>(b.height / Level::TILE_SIZE);
        auto put = [&](int x, int y, sf::Color color) {
            const int px = rooms[ii].position.x + x - world.left;
            const int py = world.top + world.height - 1 - (rooms[ii].position.y + y);
            image.setPixel(static_cast<unsigned>(px), static_cast<unsigned>(py), color);
        };
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                sf::Color color = colors.air;
                if (level.isHookBlock(x, y)) {
                    color = HOOK;
                } else if (level.isSolid(x, y)) {
                    const bool open = !level.isSolid(x - 1, y) || !level.isSolid(x + 1, y) || !level.isSolid(x, y - 1) || !level.isSolid(x, y + 1);
                    color = open ? colors.edge : colors.rock;
                } else if (level.isDangerous(x, y)) {
                    color = HAZARD;
                } else if (level.isPlatform(x, y)) {
                    color = PLATFORM;
                }
                put(x, y, color);
            }
        }
        for (const sf::Vector2f& cp : level.checkpoints()) {
            put(static_cast<int>(cp.x / Level::TILE_SIZE), static_cast<int>(cp.y / Level::TILE_SIZE), CHECKPOINT);
        }
    }
    for (size_t zz = 0; zz < ZOOMS.size(); ++zz) {
        textures[zz].loadFromImage(ZOOMS[zz] == 1 ? image : shrink(image, ZOOMS[zz]));
    }
}

void WorldMap::draw(sf::RenderTarget& target, const View& view) const
{
    const auto zoomIndex = static_cast<size_t>(std::find(ZOOMS.begin(), ZOOMS.end(), view.zoom) - ZOOMS.begin());
    if (zoomIndex >= ZOOMS.size()) return;

    // Only inside the area.
    const sf::Vector2f screen(target.getSize());
    sf::View clip(view.area);
    clip.setViewport({view.area.left / screen.x, view.area.top / screen.y, view.area.width / screen.x, view.area.height / screen.y});
    target.setView(clip);

    sf::Sprite sprite(textures[zoomIndex]);
    const sf::Vector2f topLeft = toScreen(sf::Vector2f(float(world.left), float(world.top + world.height)), view);
    sprite.setPosition(std::round(topLeft.x), std::round(topLeft.y));
    target.draw(sprite);
    target.setView(target.getDefaultView());
}

sf::Vector2f WorldMap::toScreen(sf::Vector2f tile, const View& view) const
{
    const sf::Vector2f middle(view.area.left + view.area.width * 0.5f, view.area.top + view.area.height * 0.5f);
    return middle + sf::Vector2f(tile.x - view.centre.x, view.centre.y - tile.y) / float(view.zoom);
}

sf::Vector2f WorldMap::toWorld(sf::Vector2f screen, const View& view) const
{
    const sf::Vector2f middle(view.area.left + view.area.width * 0.5f, view.area.top + view.area.height * 0.5f);
    const sf::Vector2f d = (screen - middle) * float(view.zoom);
    return {view.centre.x + d.x, view.centre.y - d.y};
}

std::optional<size_t> WorldMap::roomAt(sf::Vector2f tile) const
{
    for (size_t ii = 0; ii < rects.size(); ++ii) {
        if (rects[ii].contains(tile)) return ii;
    }
    return std::nullopt;
}

sf::FloatRect WorldMap::roomRect(size_t index) const
{
    return rects[index];
}

sf::FloatRect WorldMap::shownBounds() const
{
    float left = 0.f, bottom = 0.f, right = 0.f, top = 0.f;
    bool any = false;
    for (size_t ii = 0; ii < rects.size(); ++ii) {
        if (ii >= shown.size() || !shown[ii]) continue;
        const sf::FloatRect& r = rects[ii];
        left = any ? std::min(left, r.left) : r.left;
        bottom = any ? std::min(bottom, r.top) : r.top;
        right = any ? std::max(right, r.left + r.width) : r.left + r.width;
        top = any ? std::max(top, r.top + r.height) : r.top + r.height;
        any = true;
    }
    return {left, bottom, right - left, top - bottom};
}
