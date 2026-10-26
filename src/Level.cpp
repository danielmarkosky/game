#include "Level.h"
#include "src/Props.h"
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
    // The tiles in text: '%' metal, '#' rock, 'x' crystal, '^' spikes, '-' platform, '=' gate,
    // 'o' slick, '~' ice, '&' crumbling, '_' cloud, 'w' water, 'm' bounce mushroom, '?' hidden
    // passage, 't' rails (see Level::TILE_ROCK and the rest).

    // Where things are in the sprite sheets made by tools/make_art.py.
    namespace sheet
    {
        // assets/gfx/themes/<theme>/tiles.png, 8x8 tiles, 8 per row. Every theme has them in the same places.
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
        constexpr int GATE = 24;
        constexpr int SLICK = 25;
        constexpr int ICE = 26;
        constexpr int CRUMBLE = 27;
        constexpr int CLOUD = 28;
        constexpr int BOUNCE = 29;
        constexpr int RAIL = 30;

        // assets/gfx/objects.png, 8x24 frames.
        constexpr int OBJECT_WIDTH = 8;
        constexpr int OBJECT_HEIGHT = 24;
        constexpr int FLAG_OFF = 0;
        constexpr int FLAG_ON = 1;     // 2 frames
        constexpr int GOAL = 3;        // 3 frames

        // assets/gfx/items.png, 10x10 frames, in the order of ITEMS.
        constexpr int ITEM_SIZE = 10;
        constexpr int GEM = 1;
        // The frame of each ability's pickup.
        const std::pair<const char*, int> ITEMS[] = {{"rope", 0}, {"long_rope", 2}, {"double_jump", 5}, {"wall_climb", 6}, {"air_throw", 7}, {"stopwatch", 8}};
        constexpr int LEVER_UP = 3, LEVER_DOWN = 4;
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
    loadTextures();
}

Level::Level(const std::unordered_map<int, std::unordered_map<int, int>>& tiles)
{
    m_tileMap = tiles;
    computeBounds();
    loadTextures();
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

    auto level = fromRows(rows);
    if (!level->hasStart()) throw std::runtime_error("Level has no start position ('S')");
    return level;
}

std::shared_ptr<Level> Level::fromRows(const std::vector<std::string>& rows)
{
    auto level = std::make_shared<Level>(std::unordered_map<int, std::unordered_map<int, int>>{});
    const int numRows = static_cast<int>(rows.size());
    for (int row = 0; row < numRows; ++row) {
        // The first line of the file is the top of the level, and world y points up.
        const int y = numRows - 1 - row;
        for (int x = 0; x < static_cast<int>(rows[row].size()); ++x) {
            switch (rows[row][x]) {
                case '#': level->m_tileMap[x][y] = TILE_ROCK; break;
                case '%': level->m_tileMap[x][y] = TILE_METAL; break;
                case '^': level->m_tileMap[x][y] = TILE_SPIKES; break;
                case 'x': level->m_tileMap[x][y] = TILE_CRYSTAL; break;
                case '-': level->m_tileMap[x][y] = TILE_PLATFORM; break;
                case '=': level->m_tileMap[x][y] = TILE_GATE; break;
                case 'o': level->m_tileMap[x][y] = TILE_SLICK; break;
                case '~': level->m_tileMap[x][y] = TILE_ICE; break;
                case '&': level->m_tileMap[x][y] = TILE_CRUMBLE; break;
                case '_': level->m_tileMap[x][y] = TILE_CLOUD; break;
                case 'w': level->m_tileMap[x][y] = TILE_WATER; break;
                case 'm': level->m_tileMap[x][y] = TILE_BOUNCE; break;
                case '?': level->m_tileMap[x][y] = TILE_HIDDEN; break;
                case 't': level->m_tileMap[x][y] = TILE_RAIL; break;
                case 'K': level->addCart({x, y}); break;
                case 'E': level->addCartStop({x, y}); break;
                case 'Y': level->addIcicle({x, y}); break;
                case 'U': level->addUpdraft({x, y}); break;
                case 'Q': level->addLamp({x, y}); break;
                case 'R': level->addRing({x, y}, {x + 6, y}, 4.f); break;
                case 'L': level->addLever({x, y}); break;
                case 'S': level->setStart({x, y}); break;
                case 'C': level->addCheckpoint({x, y}); break;
                case 'G': level->setGoal({x, y}); break;
                case 'P': level->addPickup("rope", {x, y}); break;
                case '*': level->addGem({x, y}); break;
                case 'N': level->addNpc("", "", {x, y}); break; // says nothing: the fields come from LDtk
                case 'B': level->addBench({x, y}); break;
                default: break;
            }
        }
    }
    level->computeBounds();
    return level;
}

void Level::setStart(sf::Vector2i tile)
{
    m_startPos = sf::Vector2f(tile) * TILE_SIZE;
}

void Level::addCheckpoint(sf::Vector2i tile)
{
    m_checkpoints.push_back(sf::Vector2f(tile) * TILE_SIZE);
}

void Level::setGoal(sf::Vector2i tile)
{
    m_goal = sf::Vector2f(tile) * TILE_SIZE;
}

void Level::addPickup(const std::string& ability, sf::Vector2i tile)
{
    m_pickups.push_back({ability, sf::Vector2f(tile) * TILE_SIZE});
}

void Level::addGem(sf::Vector2i tile)
{
    m_gems.push_back(sf::Vector2f(tile) * TILE_SIZE);
}

void Level::addNpc(const Npc& npc, sf::Vector2i tile)
{
    m_npcs.push_back(npc);
    m_npcs.back().position = sf::Vector2f(tile) * TILE_SIZE;
}

void Level::addNpc(const std::string& dialogue, const std::string& look, sf::Vector2i tile, bool sitting)
{
    addNpc(Npc{dialogue, look, {}, sitting, "", ""}, tile);
}

void Level::addBench(sf::Vector2i tile)
{
    m_benches.push_back(sf::Vector2f(tile) * TILE_SIZE);
}

void Level::addLever(sf::Vector2i tile)
{
    m_levers.push_back(sf::Vector2f(tile) * TILE_SIZE);
}

void Level::addProp(const std::string& kind, sf::Vector2i tile, const std::string& needs)
{
    m_props.push_back({kind, sf::Vector2f(tile) * TILE_SIZE, needs});
}

const std::vector<sf::Vector2f>& Level::levers() const
{
    return m_levers;
}

void Level::setGatesOpen(bool open)
{
    m_gatesOpen = open;
}

bool Level::gatesOpen() const
{
    return m_gatesOpen;
}

void Level::drawLevers(sf::RenderTarget& target, sf::Vector2f centr) const
{
    sf::Sprite sprite(m_items);
    const int frame = m_gatesOpen ? sheet::LEVER_DOWN : sheet::LEVER_UP;
    sprite.setTextureRect(sf::IntRect(frame * sheet::ITEM_SIZE, 0, sheet::ITEM_SIZE, sheet::ITEM_SIZE));
    for (const sf::Vector2f& lever : m_levers) {
        auto pos = viewCoordinates(lever + sf::Vector2f(-1.f, TILE_SIZE + 1.f), centr);
        sprite.setPosition(std::floor(pos.x), std::floor(pos.y));
        target.draw(sprite);
    }
}

void Level::drawProps(sf::RenderTarget& target, sf::Vector2f centr, float time, const std::function<bool(const std::string&)>& holds) const
{
    sf::Sprite sprite(props::texture());
    for (const Prop& prop : m_props) {
        const bool on = prop.needs.empty() || holds(prop.needs);
        const PropInfo* info = props::find(on ? prop.kind : prop.kind + "_off");
        if (!info) continue;
        // Animated ones not all in step.
        const int frame = info->frames > 1 ? static_cast<int>(time * info->fps + prop.position.x * 0.13f) % info->frames : 0;
        sprite.setTextureRect({info->rect.left + frame * info->rect.width, info->rect.top, info->rect.width, info->rect.height});
        // Standing on the bottom of the tile, or hanging from its top; the outline is outside both.
        const float w = static_cast<float>(info->rect.width), h = static_cast<float>(info->rect.height);
        sprite.setOrigin(std::floor(w * 0.5f), info->hangs ? 1.f : h - 1.f);
        const sf::Vector2f at = viewCoordinates(prop.position + sf::Vector2f(TILE_SIZE * 0.5f, info->hangs ? TILE_SIZE : 0.f), centr);
        sprite.setPosition(std::floor(at.x), std::floor(at.y));
        target.draw(sprite);
    }
}

void Level::loadTextures()
{
    m_theme = Theme::get(Theme::DEFAULT);
    m_objects.loadFromFile(config::assetPath("gfx/objects.png"));
    m_items.loadFromFile(config::assetPath("gfx/items.png"));
    m_bench.loadFromFile(config::assetPath("gfx/bench.png"));
}

void Level::setTheme(std::shared_ptr<const Theme> theme)
{
    m_theme = std::move(theme);
}

const Theme& Level::theme() const
{
    return *m_theme;
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

void Level::drawRock(sf::Sprite& sprite, sf::RenderTarget& target, sf::Vector2f centr, int x, int y) const
{
    // Picked from the neighbours: grass on top, outlines on exposed sides, darker rock deep inside.
    // Beyond the edges of the level counts as rock, for the looks: the room next door carries on
    // there, so its outer walls get no outline or grass, and two rooms side by side join up. So
    // do hidden passages: nothing about the rock round one gives it away.
    const int left = static_cast<int>(m_bounds.left / TILE_SIZE), bottom = static_cast<int>(m_bounds.top / TILE_SIZE);
    const int right = left + static_cast<int>(m_bounds.width / TILE_SIZE), top = bottom + static_cast<int>(m_bounds.height / TILE_SIZE);
    auto looksSolid = [&](int tx, int ty) { return tx < left || tx >= right || ty < bottom || ty >= top || isSolid(tx, ty) || isHidden(tx, ty); };
    const int size = static_cast<int>(TILE_SIZE);
    auto drawTile = [&](int index, int tx, int ty) {
        auto pos = viewCoordinates(sf::Vector2f(tx * TILE_SIZE, (ty + 1) * TILE_SIZE), centr);
        sprite.setTextureRect(sf::IntRect((index % sheet::COLUMNS) * size, (index / sheet::COLUMNS) * size, size, size));
        sprite.setPosition(std::floor(pos.x), std::floor(pos.y));
        target.draw(sprite);
    };
    const unsigned hash = tileHash(x, y);
    const bool openTop = !looksSolid(x, y + 1), openBottom = !looksSolid(x, y - 1);
    const bool openLeft = !looksSolid(x - 1, y), openRight = !looksSolid(x + 1, y);
    drawTile((openTop || openBottom || openLeft || openRight) ? sheet::ROCK + static_cast<int>(hash % 3) : sheet::ROCK_DEEP, x, y);
    if (openLeft) drawTile(sheet::EDGE_LEFT, x, y);
    if (openRight) drawTile(sheet::EDGE_RIGHT, x, y);
    if (openBottom) drawTile(sheet::EDGE_BOTTOM, x, y);
    if (openTop) {
        drawTile(sheet::GRASS + static_cast<int>((hash >> 4) % 2), x, y);
        if (tileAt(x, y + 1) == 0) {
            if (hash % 9 == 0) drawTile(sheet::FLOWER, x, y + 1);
            else if (hash % 4 == 0) drawTile(sheet::TUFT, x, y + 1);
        }
    }
    if (openBottom && tileAt(x, y - 1) == 0 && hash % 5 == 0) drawTile(sheet::DRIP, x, y - 1);
}

void Level::draw(sf::RenderTarget& target, sf::Vector2f centr) const
{
    // Only visit the tiles that can be on screen.
    const sf::Vector2f halfScreen(config::GAME_WIDTH * 0.5f, config::GAME_HEIGHT * 0.5f);
    auto [x0, y0] = tailNumbers(centr - halfScreen);
    auto [x1, y1] = tailNumbers(centr + halfScreen);

    sf::Sprite sprite(m_theme->tiles);
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

            if (tile == TILE_ROCK) {
                drawRock(sprite, target, centr, x, y);
            } else if (tile == TILE_METAL) {
                drawTile(sheet::HOOK_BLOCK, x, y);
            } else if (isDangerous(x, y)) {
                const int first = tile == TILE_CRYSTAL ? sheet::CRYSTALS : sheet::SPIKES;
                drawTile(first + static_cast<int>(spikeDir(*this, x, y)), x, y);
            } else if (tile == TILE_SLICK) {
                drawTile(sheet::SLICK, x, y);
            } else if (tile == TILE_ICE) {
                drawTile(sheet::ICE, x, y);
            } else if (tile == TILE_BOUNCE) {
                drawTile(sheet::BOUNCE, x, y);
            } else if (tile == TILE_RAIL) {
                drawTile(sheet::RAIL, x, y);
            } else if (tile == TILE_CRUMBLE || tile == TILE_CLOUD) {
                // Shaking as it goes, gone for a while, then back.
                const Crumbling* state = crumbling(x, y);
                if (state && state->gone > 0.f) continue;
                const float shake = state && state->stood > 0.f ? std::round(std::sin(state->stood * 60.f)) : 0.f;
                auto pos = viewCoordinates(sf::Vector2f(x * TILE_SIZE + shake, (y + 1) * TILE_SIZE), centr);
                const int index = tile == TILE_CRUMBLE ? sheet::CRUMBLE : sheet::CLOUD;
                sprite.setTextureRect(sf::IntRect((index % sheet::COLUMNS) * size, (index / sheet::COLUMNS) * size, size, size));
                sprite.setPosition(std::floor(pos.x), std::floor(pos.y));
                if (tile == TILE_CLOUD && state && state->stood > 0.f) sprite.setColor(sf::Color(255, 255, 255, 150));
                target.draw(sprite);
                sprite.setColor(sf::Color::White);
            } else if (tile == TILE_GATE) {
                if (!m_gatesOpen) drawTile(sheet::GATE, x, y);
            } else if (tile == TILE_PLATFORM) {
                const bool openLeft = !isPlatform(x - 1, y) && !isSolid(x - 1, y);
                const bool openRight = !isPlatform(x + 1, y) && !isSolid(x + 1, y);
                drawTile(openLeft ? sheet::PLANK_LEFT : openRight ? sheet::PLANK_RIGHT : sheet::PLANK_MID, x, y);
            }
        }
    }
}

void Level::drawHidden(sf::RenderTarget& target, sf::Vector2f centr, sf::Vector2f player) const
{
    const sf::Vector2f halfScreen(config::GAME_WIDTH * 0.5f, config::GAME_HEIGHT * 0.5f);
    auto [x0, y0] = tailNumbers(centr - halfScreen);
    auto [x1, y1] = tailNumbers(centr + halfScreen);
    sf::Sprite sprite(m_theme->tiles);
    for (int x = x0 - 1; x <= x1 + 1; ++x) {
        for (int y = y0 - 1; y <= y1 + 1; ++y) {
            if (!isHidden(x, y)) continue;
            // In one, it fades: most near you, so you can see your way through.
            const float d = distance(player, {(x + 0.5f) * TILE_SIZE, (y + 0.5f) * TILE_SIZE});
            const float near = std::clamp(1.f - (d - 16.f) / 40.f, 0.f, 1.f);
            sprite.setColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(255.f * (1.f - m_reveal * (0.35f + 0.45f * near)))));
            drawRock(sprite, target, centr, x, y);
        }
    }
}

void Level::drawMarkers(sf::RenderTarget& target, sf::Vector2f centr, std::optional<sf::Vector2f> activeCheckpoint, float time) const
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

void Level::drawPickups(sf::RenderTarget& target, sf::Vector2f centr, float time, const std::function<bool(const std::string&)>& owned) const
{
    sf::Sprite sprite(m_items);
    for (const Pickup& pickup : m_pickups) {
        if (owned(pickup.ability)) continue;
        const auto it = std::find_if(std::begin(sheet::ITEMS), std::end(sheet::ITEMS), [&](const auto& item) { return pickup.ability == item.first; });
        const int frame = it == std::end(sheet::ITEMS) ? 0 : it->second;
        sprite.setTextureRect(sf::IntRect(frame * sheet::ITEM_SIZE, 0, sheet::ITEM_SIZE, sheet::ITEM_SIZE));
        // Floats in the middle of its tile, a pixel or two above the ground.
        const float bob = std::round(std::sin(time * 3.f) * 1.5f);
        auto pos = viewCoordinates(pickup.position + sf::Vector2f(-1.f, TILE_SIZE + 3.f + bob), centr);
        // A soft glow behind it, pulsing, so it catches the eye.
        const float glow = 0.5f + 0.5f * std::sin(time * 4.f);
        sf::CircleShape halo(7.f);
        halo.setOrigin(7.f, 7.f);
        halo.setPosition(std::floor(pos.x) + 5.f, std::floor(pos.y) + 5.f);
        halo.setFillColor(sf::Color(255, 220, 120, static_cast<sf::Uint8>(22.f + 22.f * glow)));
        target.draw(halo);
        sprite.setPosition(std::floor(pos.x), std::floor(pos.y));
        target.draw(sprite);
    }
}

void Level::drawBenches(sf::RenderTarget& target, sf::Vector2f centr, std::optional<size_t> glowing, float time) const
{
    sf::Sprite sprite(m_bench);
    for (size_t ii = 0; ii < m_benches.size(); ++ii) {
        // The sprite has a 1 px outline all round, and stands on the bottom of the tile.
        auto pos = viewCoordinates(m_benches[ii] + sf::Vector2f(-1.f, sprite.getLocalBounds().height - 1.f), centr);
        if (glowing == ii) {
            const float glow = 0.5f + 0.5f * std::sin(time * 2.f);
            sf::CircleShape halo(13.f);
            halo.setOrigin(13.f, 13.f);
            halo.setPosition(std::floor(pos.x) + 9.f, std::floor(pos.y) + 6.f);
            halo.setFillColor(sf::Color(255, 210, 120, static_cast<sf::Uint8>(15.f + 15.f * glow)));
            target.draw(halo);
        }
        sprite.setPosition(std::floor(pos.x), std::floor(pos.y));
        target.draw(sprite);
    }
}

void Level::drawGems(sf::RenderTarget& target, sf::Vector2f centr, float time, const std::function<bool(size_t)>& collected) const
{
    sf::Sprite sprite(m_items, sf::IntRect(sheet::GEM * sheet::ITEM_SIZE, 0, sheet::ITEM_SIZE, sheet::ITEM_SIZE));
    for (size_t ii = 0; ii < m_gems.size(); ++ii) {
        // Bobbing out of step with each other, and glinting now and then.
        const float phase = time * 2.5f + static_cast<float>(ii) * 1.3f;
        const float bob = std::round(std::sin(phase) * 1.f);
        auto pos = viewCoordinates(m_gems[ii] + sf::Vector2f(-1.f, TILE_SIZE + 1.f + bob), centr);
        const bool glint = std::fmod(phase, 6.f) < 0.25f;
        sprite.setColor(collected(ii) ? sf::Color(255, 255, 255, 60) : glint ? sf::Color(255, 255, 255) : sf::Color(225, 235, 240));
        sprite.setPosition(std::floor(pos.x), std::floor(pos.y));
        target.draw(sprite);
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

bool Level::isSlick(int x, int y) const
{
    return tileAt(x, y) == TILE_SLICK;
}

bool Level::isBounce(int x, int y) const
{
    return tileAt(x, y) == TILE_BOUNCE;
}

bool Level::isHidden(int x, int y) const
{
    return tileAt(x, y) == TILE_HIDDEN;
}

bool Level::isRail(int x, int y) const
{
    return tileAt(x, y) == TILE_RAIL;
}

bool Level::isOnBounce(sf::Vector2f feet, float width) const
{
    const float row = feet.y / TILE_SIZE;
    if (row != std::floor(row)) return false; // only exactly on a tile's top edge
    return anyTileIn(feet - sf::Vector2f(0.f, 1.f), 1.f, width, [this](int x, int y) { return isBounce(x, y); });
}

bool Level::isHookBlock(int x, int y) const
{
    return tileAt(x, y) == TILE_METAL;
}

bool Level::isSolid(int x, int y) const
{
    int tile = this->tileAt(x, y);
    if (tile == TILE_CRUMBLE) {
        const Crumbling* state = crumbling(x, y);
        return !state || state->gone <= 0.f;
    }
    return tile == TILE_ROCK || tile == TILE_METAL || tile == TILE_SLICK || tile == TILE_ICE || tile == TILE_BOUNCE ||
           (tile == TILE_GATE && !m_gatesOpen);
}

bool Level::isDangerous(int x, int y) const
{
    int tile = this->tileAt(x, y);
    return tile == TILE_SPIKES or tile == TILE_CRYSTAL;
}

bool Level::isPlatform(int x, int y) const
{
    const int tile = tileAt(x, y);
    if (tile == TILE_CLOUD) {
        const Crumbling* state = crumbling(x, y);
        return !state || state->gone <= 0.f;
    }
    return tile == TILE_PLATFORM || tile == TILE_RAIL;
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
    // Icicles, hanging or falling: their sharp end.
    for (const Icicle& icicle : m_icicles) {
        if (icicle.state == Icicle::State::Gone) continue;
        if (overlaps(pos, height, width, {icicle.tip.x - 2.f, icicle.tip.y, 4.f, 8.f})) return true;
    }
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

const std::vector<Pickup>& Level::pickups() const
{
    return m_pickups;
}

const Pickup* Level::pickupAt(sf::Vector2f pos, float height, float width) const
{
    for (const auto& pickup : m_pickups) {
        if (overlaps(pos, height, width, markerArea(pickup.position))) return &pickup;
    }
    return nullptr;
}

const std::vector<sf::Vector2f>& Level::gems() const
{
    return m_gems;
}

std::optional<size_t> Level::gemAt(sf::Vector2f pos, float height, float width) const
{
    for (size_t ii = 0; ii < m_gems.size(); ++ii) {
        if (overlaps(pos, height, width, {m_gems[ii].x, m_gems[ii].y, TILE_SIZE, TILE_SIZE})) return ii;
    }
    return std::nullopt;
}

const std::vector<Npc>& Level::npcs() const
{
    return m_npcs;
}

const std::vector<Prop>& Level::props() const
{
    return m_props;
}

const std::vector<sf::Vector2f>& Level::benches() const
{
    return m_benches;
}

std::optional<Level::Side> Level::exitThrough(sf::Vector2f point) const
{
    if (point.x < m_bounds.left) return Side::Left;
    if (point.x > m_bounds.left + m_bounds.width) return Side::Right;
    if (point.y > m_bounds.top + m_bounds.height) return Side::Top;
    if (point.y < m_bounds.top) return Side::Bottom;
    return std::nullopt;
}

bool Level::hasStart() const
{
    return m_startPos.has_value();
}

sf::Vector2f Level::getStartPos() const
{
    // The test room made by Level() has none: somewhere inside it.
    return m_startPos.value_or(sf::Vector2f(100.f, 100.f));
}

const std::vector<sf::Vector2f>& Level::checkpoints() const
{
    return m_checkpoints;
}

sf::FloatRect Level::bounds() const
{
    return m_bounds;
}
