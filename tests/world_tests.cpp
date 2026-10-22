#include <gtest/gtest.h>
#include "src/Config.h"
#include "src/Game.h"
#include "src/Level.h"
#include <cmath>
#include <optional>
#include <set>
#include <vector>

// The shipped levels.txt as one world map, in tiles.
namespace
{
    struct Room
    {
        LevelInfo info;
        std::shared_ptr<Level> level;
        int left, bottom, right, top; // world tiles, right and top exclusive
    };

    std::vector<Room> loadWorld()
    {
        std::vector<Room> rooms;
        for (const auto& info : loadLevelList(config::assetPath("levels/levels.txt"))) {
            auto level = Level::loadFromFile(info.path);
            const sf::FloatRect b = level->bounds();
            const int x0 = static_cast<int>(b.left / Level::TILE_SIZE), y0 = static_cast<int>(b.top / Level::TILE_SIZE);
            const int w = static_cast<int>(b.width / Level::TILE_SIZE), h = static_cast<int>(b.height / Level::TILE_SIZE);
            rooms.push_back({info, level, info.position.x + x0, info.position.y + y0, info.position.x + x0 + w, info.position.y + y0 + h});
        }
        return rooms;
    }

    std::optional<size_t> roomAt(const std::vector<Room>& rooms, int x, int y)
    {
        for (size_t ii = 0; ii < rooms.size(); ++ii) {
            const Room& r = rooms[ii];
            if (x >= r.left && x < r.right && y >= r.bottom && y < r.top) return ii;
        }
        return std::nullopt;
    }

    bool solidAt(const Room& room, int x, int y)
    {
        return room.level->isSolid(x - room.info.position.x, y - room.info.position.y);
    }
}

TEST(World, RoomsDontOverlap) {
    const auto rooms = loadWorld();
    for (size_t a = 0; a < rooms.size(); ++a) {
        for (size_t b = a + 1; b < rooms.size(); ++b) {
            const Room &ra = rooms[a], &rb = rooms[b];
            const bool overlap = ra.left < rb.right && rb.left < ra.right && ra.bottom < rb.top && rb.bottom < ra.top;
            EXPECT_FALSE(overlap) << ra.info.path << " and " << rb.info.path;
        }
    }
}

TEST(World, OpeningsLineUp) {
    // Every gap in the edge of a room must meet a gap in the room next to it, or the player would
    // come out inside rock. A gap leading nowhere is only allowed at the bottom (a pit) and in the
    // last room (the end of the game).
    const auto rooms = loadWorld();
    for (size_t ii = 0; ii < rooms.size(); ++ii) {
        const Room& r = rooms[ii];
        SCOPED_TRACE(r.info.path);
        auto check = [&](int x, int y, int dx, int dy) {
            if (solidAt(r, x, y)) return;
            const auto other = roomAt(rooms, x + dx, y + dy);
            if (other) {
                EXPECT_FALSE(solidAt(rooms[*other], x + dx, y + dy))
                    << "opening at world tile " << x << "," << y << " runs into rock in " << rooms[*other].info.path;
            } else if (dy >= 0) {
                EXPECT_EQ(ii, rooms.size() - 1) << "opening at world tile " << x << "," << y << " leads nowhere";
            }
        };
        for (int y = r.bottom; y < r.top; ++y) {
            check(r.left, y, -1, 0);
            check(r.right - 1, y, 1, 0);
        }
        for (int x = r.left; x < r.right; ++x) {
            check(x, r.top - 1, 0, 1);
            check(x, r.bottom, 0, -1);
        }
    }
}

TEST(World, EveryRoomIsConnectedAndTheEndCanBeReached) {
    // Flood fills the empty tiles of the whole world from the first room's start. (This doesn't
    // check the jumps are possible.)
    const auto rooms = loadWorld();
    const sf::Vector2f start = rooms[0].level->getStartPos();
    std::vector<std::pair<int, int>> todo = {{rooms[0].info.position.x + static_cast<int>(start.x / Level::TILE_SIZE),
                                              rooms[0].info.position.y + static_cast<int>(start.y / Level::TILE_SIZE)}};
    std::set<std::pair<int, int>> seen(todo.begin(), todo.end());
    std::set<size_t> roomsSeen;
    bool foundEnd = false;
    while (!todo.empty()) {
        auto [x, y] = todo.back();
        todo.pop_back();
        for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            const std::pair<int, int> next{x + dx, y + dy};
            const auto room = roomAt(rooms, next.first, next.second);
            if (!room) {
                if (dy >= 0) foundEnd = true; // out of the world, not down a pit
                continue;
            }
            if (solidAt(rooms[*room], next.first, next.second) || !seen.insert(next).second) continue;
            roomsSeen.insert(*room);
            todo.push_back(next);
        }
    }
    EXPECT_EQ(roomsSeen.size(), rooms.size());
    EXPECT_TRUE(foundEnd);
}
