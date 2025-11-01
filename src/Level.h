#pragma once

#include <SFML/Graphics.hpp>
#include <unordered_map>

class Level
{
public:
    static constexpr float TILE_SIZE = 8; // 8x8 tiles

    Level();
    Level(const std::unordered_map<int, std::unordered_map<int, int>>& tiles);
    void draw(sf::RenderTarget& target, sf::Vector2f centr);
    bool isSolid(int x, int y) const;
    bool isEmptySpace(sf::Vector2f pos, float hight, float width) const;

private:
    std::unordered_map<int, std::unordered_map<int, int>> m_tileMap;
    sf::Texture m_tileset;
    sf::Sprite m_tileSprite;
};
