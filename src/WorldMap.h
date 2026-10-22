#pragma once

#include "src/Level.h"
#include <SFML/Graphics.hpp>
#include <array>
#include <memory>
#include <optional>
#include <vector>

// The map of the world: the rooms you've been in, drawn small, one pixel per tile or less.
// Everything here is in world tiles (x to the right, y up), not pixels.
class WorldMap
{
public:
    // How far out each zoom level is: tiles per map pixel.
    static constexpr std::array<int, 3> ZOOMS = {1, 2, 4};

    struct Room
    {
        std::shared_ptr<const Level> level;
        sf::Vector2i position; // of the level's tile (0, 0)
    };

    // What part of the world is drawn where.
    struct View
    {
        sf::FloatRect area;  // on the screen
        sf::Vector2f centre; // the world tile in the middle of `area`
        int zoom = 1;        // one of ZOOMS
    };

    explicit WorldMap(std::vector<Room> rooms);

    // Which rooms are drawn, one flag per room. Only redraws the map when that changed.
    void reveal(const std::vector<bool>& shown);
    void draw(sf::RenderTarget& target, const View& view) const;

    sf::Vector2f toScreen(sf::Vector2f tile, const View& view) const;
    sf::Vector2f toWorld(sf::Vector2f screen, const View& view) const;
    std::optional<size_t> roomAt(sf::Vector2f tile) const;
    // The room's tiles in the world: left, bottom, width, height.
    sf::FloatRect roomRect(size_t index) const;
    // Around all the rooms that are shown.
    sf::FloatRect shownBounds() const;

private:
    struct Colors
    {
        sf::Color rock, edge, air;
    };
    static Colors themeColors(const Theme& theme);

    std::vector<Room> rooms;
    std::vector<sf::FloatRect> rects;
    sf::IntRect world; // around every room: left, bottom, width, height
    std::vector<bool> shown;
    std::array<sf::Texture, ZOOMS.size()> textures; // the shown rooms at each zoom level
};
