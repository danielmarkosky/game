#pragma once

#include "src/Theme.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Something lying in a level that gives the player an ability when touched.
struct Pickup
{
    std::string ability;   // e.g. "climbing_pick"
    sf::Vector2f position; // bottom-left of its tile
};

class Level
{
public:
    static constexpr float TILE_SIZE = 8; // 8x8 tiles

    // The edges of a level. Going through a gap in one takes you into the room on the other side,
    // if there is one. Falling out of the bottom with no room below kills you.
    enum class Side { Left, Right, Top, Bottom };

    // A small bordered test room.
    Level();
    Level(const std::unordered_map<int, std::unordered_map<int, int>>& tiles);

    // Loads a level from a text file. See assets/levels/README.md for the format.
    // Throws std::runtime_error if the file can't be read or has no start position.
    static std::shared_ptr<Level> loadFromFile(const std::string& path);
    static std::shared_ptr<Level> loadFromString(const std::string& text);

    // The tiles are drawn with the theme's tiles.png. New levels use Theme::DEFAULT.
    void setTheme(std::shared_ptr<const Theme> theme);
    const Theme& theme() const;

    void draw(sf::RenderTarget& target, sf::Vector2f centr) const;
    void drawMarkers(sf::RenderTarget& target, sf::Vector2f centr, std::optional<sf::Vector2f> activeCheckpoint, float time) const;
    // The pickups whose ability `owned` says the player doesn't have yet, bobbing up and down.
    void drawPickups(sf::RenderTarget& target, sf::Vector2f centr, float time, const std::function<bool(const std::string&)>& owned) const;
    // The gems, the ones `collected` says were found already as faint ghosts.
    void drawGems(sf::RenderTarget& target, sf::Vector2f centr, float time, const std::function<bool(size_t)>& collected) const;

    int tileAt(int x, int y) const;
    bool isSolid(int x, int y) const;
    bool isHookBlock(int x, int y) const; // '%'
    bool isDangerous(int x, int y) const;
    // One-way platforms: you can jump up through them, but land on their top.
    bool isPlatform(int x, int y) const;
    // True if a box of `width` with its bottom at `feet` stands exactly on top of a platform.
    bool isOnPlatform(sf::Vector2f feet, float width) const;
    bool isEmptySpace(sf::Vector2f pos, float height, float width) const;
    // True if the box touches the pointy part of a spike (the half of the tile next to its rock).
    bool isDangerousSpace(sf::Vector2f pos, float height, float width) const;

    // Respawn position of the checkpoint the box touches, if any.
    std::optional<sf::Vector2f> checkpointAt(sf::Vector2f pos, float height, float width) const;
    bool isGoal(sf::Vector2f pos, float height, float width) const;
    const std::vector<Pickup>& pickups() const;
    // The pickup the box touches, if any.
    const Pickup* pickupAt(sf::Vector2f pos, float height, float width) const;
    // Gems are collectibles, nothing more. Each is the bottom-left of its tile.
    const std::vector<sf::Vector2f>& gems() const;
    // The index of the gem the box touches, if any.
    std::optional<size_t> gemAt(sf::Vector2f pos, float height, float width) const;
    // Which edge of the level `point` has gone past, if any.
    std::optional<Side> exitThrough(sf::Vector2f point) const;

    sf::Vector2f getStartPos() const;
    const std::vector<sf::Vector2f>& checkpoints() const;
    // World-space rectangle covering every tile. `left`/`top` hold the minimum x/y, and since
    // world y points up, `top` is actually the bottom edge.
    sf::FloatRect bounds() const;

private:
    void loadTextures();
    void computeBounds();

    std::unordered_map<int, std::unordered_map<int, int>> m_tileMap;
    std::vector<sf::Vector2f> m_checkpoints;
    std::optional<sf::Vector2f> m_goal;
    std::vector<Pickup> m_pickups;
    std::vector<sf::Vector2f> m_gems;
    std::shared_ptr<const Theme> m_theme;
    sf::Texture m_objects;
    sf::Texture m_items;
    sf::Vector2f m_startPos = {100.f, 100.f};
    sf::FloatRect m_bounds;
};
