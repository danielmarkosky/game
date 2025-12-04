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
    int tileAt(int x, int y) const;
    bool isSolid(int x, int y) const;
    bool isDangerous(int x, int y) const;
    bool isEmptySpace(sf::Vector2f pos, float hight, float width) const;
	bool isDangerousSpace(sf::Vector2f pos, float hight, float width) const;
    void resetLevel();
	sf::Vector2f getStartPos() const;

private:
    std::unordered_map<int, std::unordered_map<int, int>> m_tileMap;
    sf::Texture m_tileset;
    sf::Sprite m_tileSprite;
	sf::Vector2f m_startPos = {100.f, 100.f};
};
