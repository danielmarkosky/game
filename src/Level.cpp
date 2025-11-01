#include "Level.h"
#include "utils.h"
#include <random>

Level::Level()
{
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(1, 2);

    for (int y = 0; y < 23; ++y) {
        for (int x = 0; x < 40; ++x) {
            if (y == 0 || y == 22 || x == 0 || x == 39) {
                m_tileMap[x][y] = dist(rng);
            } else {
                m_tileMap[x][y] = 0; // empty
            }
        }
    }
    // Example: add a platform
    m_tileMap[10][10] = 2;
    m_tileMap[11][10] = 1;
    m_tileMap[12][10] = 2;

    m_tileset.loadFromFile("assets/tiles_map.png");
    m_tileSprite.setTexture(m_tileset);
}

Level::Level(const std::unordered_map<int, std::unordered_map<int, int>>& tiles)
{
    m_tileMap = tiles;
    m_tileset.loadFromFile("assets/tiles_map.png");
    m_tileSprite.setTexture(m_tileset);
}

void Level::draw(sf::RenderTarget& target, sf::Vector2f centr)
{
    for (const auto& [x, col] : m_tileMap) {
        for (const auto& [y, tile] : col) {
            if (tile == 0) continue;
            m_tileSprite.setPosition(viewCoordinates(sf::Vector2f(x * TILE_SIZE, y * TILE_SIZE), centr));
            m_tileSprite.setTextureRect(sf::IntRect((tile - 1) * TILE_SIZE, 0, TILE_SIZE, TILE_SIZE));
            target.draw(m_tileSprite);
        }
    }
}

bool Level::isSolid(int x, int y) const
{
    auto itx = m_tileMap.find(x);
    if (itx == m_tileMap.end()) return false;
    auto ity = itx->second.find(y);
    if (ity == itx->second.end()) return false;
    return ity->second != 0;
}

bool Level::isEmptySpace(sf::Vector2f pos, float hight, float width) const
{
    std::pair<int, int> start = tailNumbers(pos);
    std::pair<int, int> stop  = tailNumbers(pos + sf::Vector2f(width, hight));
    for (int ii = start.first; ii <= stop.first; ii++) {
        for (int jj = start.second; jj <= stop.second; jj++) {
            if (isSolid(ii, jj)) return false;
        }
    }
    return true;
}