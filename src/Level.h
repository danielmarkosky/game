#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Level
{
public:
    static constexpr float TILE_SIZE = 8; // 8x8 tiles

    // A small bordered test room.
    Level();
    Level(const std::unordered_map<int, std::unordered_map<int, int>>& tiles);

    // Loads a level from a text file. See assets/levels/README.md for the format.
    // Throws std::runtime_error if the file can't be read or has no start position.
    static std::shared_ptr<Level> loadFromFile(const std::string& path);
    static std::shared_ptr<Level> loadFromString(const std::string& text);

    void draw(sf::RenderTarget& target, sf::Vector2f centr) const;
    void drawMarkers(sf::RenderTarget& target, sf::Vector2f centr, sf::Vector2f activeCheckpoint, float time) const;

    int tileAt(int x, int y) const;
    bool isSolid(int x, int y) const;
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

    sf::Vector2f getStartPos() const;
    // World-space rectangle covering every tile. `left`/`top` hold the minimum x/y, and since
    // world y points up, `top` is actually the bottom edge.
    sf::FloatRect bounds() const;

private:
    void loadTileset();
    void computeBounds();

    std::unordered_map<int, std::unordered_map<int, int>> m_tileMap;
    std::vector<sf::Vector2f> m_checkpoints;
    std::optional<sf::Vector2f> m_goal;
    sf::Texture m_tileset;
    sf::Texture m_objects;
    sf::Vector2f m_startPos = {100.f, 100.f};
    sf::FloatRect m_bounds;
};
