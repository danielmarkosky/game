#include "Level.h"
#include "Config.h"
#include "utils.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace
{
    // Tile ids in the tile map.
    constexpr int TILE_METAL = 1;    // '%'
    constexpr int TILE_ROCK = 2;     // '#'
    constexpr int TILE_CRYSTAL = 3;  // 'x'
    constexpr int TILE_SPIKES = 4;   // '^'
    constexpr int TILE_PLATFORM = 5; // '-'

    // Where things are in the sprite sheets made by tools/make_art.py.
    namespace sheet
    {
        // assets/gfx/tiles.png, 8x8 tiles, 8 per row.
        constexpr int COLUMNS = 8;
        constexpr int ROCK = 0;        // 3 variants
        constexpr int ROCK_DEEP = 3;
        constexpr int GRASS = 4;       // 2 variants
        constexpr int EDGE_LEFT = 6;
        constexpr int EDGE_RIGHT = 7;
        constexpr int EDGE_BOTTOM = 8;
        constexpr int TUFT = 9;
        constexpr int FLOWER = 10;
        constexpr int DRIP = 11;
        constexpr int SPIKES = 12;     // 4 directions, in SpikeDir order
        constexpr int CRYSTALS = 16;   // same
        constexpr int PLANK_LEFT = 20;
        constexpr int PLANK_MID = 21;
        constexpr int PLANK_RIGHT = 22;
        constexpr int HOOK_BLOCK = 23;

        // assets/gfx/objects.png, 8x24 frames.
        constexpr int OBJECT_WIDTH = 8;
        constexpr int OBJECT_HEIGHT = 24;
        constexpr int FLAG_OFF = 0;
        constexpr int FLAG_ON = 1;     // 2 frames
        constexpr int GOAL = 3;        // 3 frames
    }

    // Spikes point away from the rock they're stuck to.
    enum class SpikeDir { Up, Down, OnLeftWall, OnRightWall };
    // Only this much of a spike tile hurts, measured from the rock it stands on.
    constexpr float SPIKE_DEPTH = 4.f;

    // Shrinks a box so overlap tests are half-open: a box ending exactly on a tile edge
    // doesn't count as touching the next tile.
    constexpr float EDGE_EPS = 0.001f;

    template <typename Pred>
    bool anyTileIn(sf::Vector2f pos, float height, float width, Pred pred)
    {
        auto [x0, y0] = tailNumbers(pos);
        auto [x1, y1] = tailNumbers(pos + sf::Vector2f(width - EDGE_EPS, height - EDGE_EPS));
        for (int ii = x0; ii <= x1; ii++) {
            for (int jj = y0; jj <= y1; jj++) {
                if (pred(ii, jj)) return true;
            }
        }
        return false;
    }

    bool overlaps(sf::Vector2f pos, float height, float width, sf::FloatRect area)
    {
        return pos.x < area.left + area.width && pos.x + width > area.left &&
               pos.y < area.top + area.height && pos.y + height > area.top;
    }

    // Trigger area of a marker standing on the tile at `tilePos`, two tiles tall.
    sf::FloatRect markerArea(sf::Vector2f tilePos)
    {
        return {tilePos.x, tilePos.y, Level::TILE_SIZE, Level::TILE_SIZE * 2.f};
    }

    SpikeDir spikeDir(const Level& level, int x, int y)
    {
        if (level.isSolid(x, y - 1)) return SpikeDir::Up;
        if (level.isSolid(x, y + 1)) return SpikeDir::Down;
        if (level.isSolid(x - 1, y)) return SpikeDir::OnLeftWall;
        if (level.isSolid(x + 1, y)) return SpikeDir::OnRightWall;
        return SpikeDir::Up;
    }

    sf::FloatRect spikeArea(int x, int y, SpikeDir dir)
    {
        const float T = Level::TILE_SIZE;
        switch (dir) {
            case SpikeDir::Up: return {x * T, y * T, T, SPIKE_DEPTH};
            case SpikeDir::Down: return {x * T, (y + 1) * T - SPIKE_DEPTH, T, SPIKE_DEPTH};
            case SpikeDir::OnLeftWall: return {x * T, y * T, SPIKE_DEPTH, T};
            case SpikeDir::OnRightWall: return {(x + 1) * T - SPIKE_DEPTH, y * T, SPIKE_DEPTH, T};
        }
        return {x * T, y * T, T, T};
    }

    // Stable pseudo-random number per tile, for picking decoration variants.
    unsigned tileHash(int x, int y)
    {
        unsigned h = static_cast<unsigned>(x) * 73856093u ^ static_cast<unsigned>(y) * 19349663u;
        h ^= h >> 13;
        h *= 0x5bd1e995u;
        h ^= h >> 15;
        return h;
    }
}

Level::Level()
{
    for (int y = 0; y < 23; ++y) {
        for (int x = 0; x < 40; ++x) {
            if (y == 0 || y == 22 || x == 0 || x == 39) {
                m_tileMap[x][y] = TILE_ROCK;
            }
        }
    }
    m_tileMap[10][5] = TILE_ROCK;
    m_tileMap[11][5] = TILE_METAL;
    m_tileMap[12][5] = TILE_ROCK;
    m_tileMap[14][5] = TILE_METAL;

    m_tileMap[17][5] = TILE_CRYSTAL;
    m_tileMap[18][5] = TILE_CRYSTAL;
    m_tileMap[19][5] = TILE_SPIKES;
    m_tileMap[20][5] = TILE_SPIKES;
    m_tileMap[21][5] = TILE_SPIKES;
    m_tileMap[22][5] = TILE_SPIKES;

    computeBounds();
    loadTileset();
}

Level::Level(const std::unordered_map<int, std::unordered_map<int, int>>& tiles)
{
    m_tileMap = tiles;
    computeBounds();
    loadTileset();
}

std::shared_ptr<Level> Level::loadFromFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Can't open level file: " + path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str());
}

std::shared_ptr<Level> Level::loadFromString(const std::string& text)
{
    std::vector<std::string> rows;
    std::istringstream stream(text);
    for (std::string line; std::getline(stream, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        rows.push_back(line);
    }
    while (!rows.empty() && rows.back().empty()) rows.pop_back();

    auto level = std::make_shared<Level>(std::unordered_map<int, std::unordered_map<int, int>>{});
    bool hasStart = false;
    const int numRows = static_cast<int>(rows.size());

    for (int row = 0; row < numRows; ++row) {
        // The first line of the file is the top of the level, and world y points up.
        const int y = numRows - 1 - row;
        for (int x = 0; x < static_cast<int>(rows[row].size()); ++x) {
            const sf::Vector2f tilePos(x * TILE_SIZE, y * TILE_SIZE);
            switch (rows[row][x]) {
                case '#': level->m_tileMap[x][y] = TILE_ROCK; break;
                case '%': level->m_tileMap[x][y] = TILE_METAL; break;
                case '^': level->m_tileMap[x][y] = TILE_SPIKES; break;
                case 'x': level->m_tileMap[x][y] = TILE_CRYSTAL; break;
                case '-': level->m_tileMap[x][y] = TILE_PLATFORM; break;
                case 'S': level->m_startPos = tilePos; hasStart = true; break;
                case 'C': level->m_checkpoints.push_back(tilePos); break;
                case 'G': level->m_goal = tilePos; break;
                default: break;
            }
        }
    }
    if (!hasStart) throw std::runtime_error("Level has no start position ('S')");

    level->computeBounds();
    return level;
}

void Level::loadTileset()
{
    m_tileset.loadFromFile(config::assetPath("gfx/tiles.png"));
    m_objects.loadFromFile(config::assetPath("gfx/objects.png"));
}

void Level::computeBounds()
{
    int minX = std::numeric_limits<int>::max(), minY = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min(), maxY = std::numeric_limits<int>::min();
    for (const auto& [x, col] : m_tileMap) {
        for (const auto& [y, tile] : col) {
            if (tile == 0) continue;
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    }
    if (minX > maxX) {
        m_bounds = {0.f, 0.f, 0.f, 0.f};
        return;
    }
    m_bounds = {minX * TILE_SIZE, minY * TILE_SIZE, (maxX - minX + 1) * TILE_SIZE, (maxY - minY + 1) * TILE_SIZE};
}

void Level::draw(sf::RenderTarget& target, sf::Vector2f centr) const
{
    // Only visit the tiles that can be on screen.
    const sf::Vector2f halfScreen(config::GAME_WIDTH * 0.5f, config::GAME_HEIGHT * 0.5f);
    auto [x0, y0] = tailNumbers(centr - halfScreen);
    auto [x1, y1] = tailNumbers(centr + halfScreen);

    sf::Sprite sprite(m_tileset);
    const int size = static_cast<int>(TILE_SIZE);
    auto drawTile = [&](int index, int x, int y) {
        auto pos = viewCoordinates(sf::Vector2f(x * TILE_SIZE, (y + 1) * TILE_SIZE), centr);
        sprite.setTextureRect(sf::IntRect((index % sheet::COLUMNS) * size, (index / sheet::COLUMNS) * size, size, size));
        sprite.setPosition(std::floor(pos.x), std::floor(pos.y));
        target.draw(sprite);
    };

    for (int x = x0 - 1; x <= x1 + 1; ++x) {
        for (int y = y0 - 1; y <= y1 + 1; ++y) {
            const int tile = tileAt(x, y);
            if (tile == 0) continue;
            const unsigned hash = tileHash(x, y);

            if (tile == TILE_ROCK) {
                // Pick the look from the neighbours: grass on top, outlines on exposed sides,
                // darker rock deep inside.
                const bool top = !isSolid(x, y + 1), bottom = !isSolid(x, y - 1);
                const bool left = !isSolid(x - 1, y), right = !isSolid(x + 1, y);
                drawTile((top || bottom || left || right) ? sheet::ROCK + static_cast<int>(hash % 3) : sheet::ROCK_DEEP, x, y);
                if (left) drawTile(sheet::EDGE_LEFT, x, y);
                if (right) drawTile(sheet::EDGE_RIGHT, x, y);
                if (bottom) drawTile(sheet::EDGE_BOTTOM, x, y);
                if (top) {
                    drawTile(sheet::GRASS + static_cast<int>((hash >> 4) % 2), x, y);
                    if (tileAt(x, y + 1) == 0) {
                        if (hash % 9 == 0) drawTile(sheet::FLOWER, x, y + 1);
                        else if (hash % 4 == 0) drawTile(sheet::TUFT, x, y + 1);
                    }
                }
                if (bottom && tileAt(x, y - 1) == 0 && hash % 5 == 0) drawTile(sheet::DRIP, x, y - 1);
            } else if (tile == TILE_METAL) {
                drawTile(sheet::HOOK_BLOCK, x, y);
            } else if (isDangerous(x, y)) {
                const int first = tile == TILE_CRYSTAL ? sheet::CRYSTALS : sheet::SPIKES;
                drawTile(first + static_cast<int>(spikeDir(*this, x, y)), x, y);
            } else if (tile == TILE_PLATFORM) {
                const bool openLeft = !isPlatform(x - 1, y) && !isSolid(x - 1, y);
                const bool openRight = !isPlatform(x + 1, y) && !isSolid(x + 1, y);
                drawTile(openLeft ? sheet::PLANK_LEFT : openRight ? sheet::PLANK_RIGHT : sheet::PLANK_MID, x, y);
            }
        }
    }
}

void Level::drawMarkers(sf::RenderTarget& target, sf::Vector2f centr, sf::Vector2f activeCheckpoint, float time) const
{
    sf::Sprite sprite(m_objects);
    auto drawObject = [&](int frame, sf::Vector2f tilePos) {
        // Objects stand on the bottom edge of their tile.
        auto pos = viewCoordinates(tilePos + sf::Vector2f(0.f, static_cast<float>(sheet::OBJECT_HEIGHT)), centr);
        sprite.setTextureRect(sf::IntRect(frame * sheet::OBJECT_WIDTH, 0, sheet::OBJECT_WIDTH, sheet::OBJECT_HEIGHT));
        sprite.setPosition(std::floor(pos.x), std::floor(pos.y));
        target.draw(sprite);
    };

    const int wave = static_cast<int>(time * 4.f) % 2;
    for (const auto& cp : m_checkpoints) {
        drawObject(cp == activeCheckpoint ? sheet::FLAG_ON + wave : sheet::FLAG_OFF, cp);
    }
    if (m_goal) drawObject(sheet::GOAL + static_cast<int>(time * 6.f) % 3, *m_goal);
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
    return tile == TILE_ROCK or tile == TILE_METAL;
}

bool Level::isDangerous(int x, int y) const
{
    int tile = this->tileAt(x, y);
    return tile == TILE_SPIKES or tile == TILE_CRYSTAL;
}

bool Level::isPlatform(int x, int y) const
{
    return tileAt(x, y) == TILE_PLATFORM;
}

bool Level::isOnPlatform(sf::Vector2f feet, float width) const
{
    const float row = feet.y / TILE_SIZE;
    if (row != std::floor(row)) return false; // only exactly on a tile's top edge
    return anyTileIn(feet - sf::Vector2f(0.f, 1.f), 1.f, width, [this](int x, int y) { return isPlatform(x, y); });
}

bool Level::isEmptySpace(sf::Vector2f pos, float height, float width) const
{
    return !anyTileIn(pos, height, width, [this](int x, int y) { return isSolid(x, y); });
}

bool Level::isDangerousSpace(sf::Vector2f pos, float height, float width) const
{
    // Only the spikes themselves hurt, not the empty part of their tile.
    return anyTileIn(pos, height, width, [&](int x, int y) {
        return isDangerous(x, y) && overlaps(pos, height, width, spikeArea(x, y, spikeDir(*this, x, y)));
    });
}

std::optional<sf::Vector2f> Level::checkpointAt(sf::Vector2f pos, float height, float width) const
{
    for (const auto& cp : m_checkpoints) {
        if (overlaps(pos, height, width, markerArea(cp))) return cp;
    }
    return std::nullopt;
}

bool Level::isGoal(sf::Vector2f pos, float height, float width) const
{
    return m_goal && overlaps(pos, height, width, markerArea(*m_goal));
}

sf::Vector2f Level::getStartPos() const
{
    return m_startPos;
}

sf::FloatRect Level::bounds() const
{
    return m_bounds;
}
