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
                m_tileMap[x][y] = 0;
            }
        }
    }
    m_tileMap[10][5] = 2;
    m_tileMap[11][5] = 1;
    m_tileMap[12][5] = 2;

    m_tileMap[14][5] = dist(rng);

    m_tileMap[17][5] = 3;
    m_tileMap[18][5] = 3;
    m_tileMap[19][5] = 4;
    m_tileMap[20][5] = 4;
    m_tileMap[21][5] = 4;
    m_tileMap[22][5] = 4;

    m_tileMap[0][1] = 0;


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
            auto pos = viewCoordinates(sf::Vector2f(x * TILE_SIZE, (y + 1) * TILE_SIZE), centr);
            m_tileSprite.setPosition(std::floor(pos.x), std::floor(pos.y));
            m_tileSprite.setTextureRect(sf::IntRect((tile - 1) * TILE_SIZE, 0, TILE_SIZE, TILE_SIZE));
            target.draw(m_tileSprite);
        }
    }
}

int Level::tileAt(int x, int y) const
{
    auto itx = m_tileMap.find(x);
    if (itx == m_tileMap.end()) return 0;
    auto ity = itx->second.find(y);
    if (ity == itx->second.end()) return 0;
    return ity->second;
}

bool Level::isSolid(int x, int y) const
{
    int tile = this->tileAt(x, y);
    return tile == 1 or tile == 2;
}

bool Level::isDangerous(int x, int y) const
{
    int tile = this->tileAt(x, y);
    return tile == 3 or tile == 4;
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

bool Level::isDangerousSpace(sf::Vector2f pos, float hight, float width) const
{
    std::pair<int, int> start = tailNumbers(pos);
    std::pair<int, int> stop  = tailNumbers(pos + sf::Vector2f(width, hight));
    for (int ii = start.first; ii <= stop.first; ii++) {
        for (int jj = start.second; jj <= stop.second; jj++) {
            if (isDangerous(ii, jj)) return true;
        }
    }
    return false;
}

void Level::resetLevel() {}

sf::Vector2f Level::getStartPos() const
{
    return m_startPos;
}
