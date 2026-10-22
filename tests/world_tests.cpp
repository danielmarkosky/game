#include <gtest/gtest.h>
#include "src/Config.h"
#include "src/World.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include "src/Level.h"
#include <cmath>
#include <optional>
#include <set>
#include <vector>

// The shipped world, assets/world.ldtk, as one map in tiles.
namespace
{
    struct Room
    {
        LevelInfo info;
        std::shared_ptr<Level> level;
        int left, bottom, right, top; // world tiles, right and top exclusive
    };

    std::vector<Room> shippedWorld()
    {
        std::vector<Room> rooms;
        for (const auto& info : loadWorld(config::assetPath("world.ldtk"))) {
            auto level = info.level;
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
    const auto rooms = shippedWorld();
    for (size_t a = 0; a < rooms.size(); ++a) {
        for (size_t b = a + 1; b < rooms.size(); ++b) {
            const Room &ra = rooms[a], &rb = rooms[b];
            const bool overlap = ra.left < rb.right && rb.left < ra.right && ra.bottom < rb.top && rb.bottom < ra.top;
            EXPECT_FALSE(overlap) << ra.info.name << " and " << rb.info.name;
        }
    }
}

TEST(World, OpeningsLineUp) {
    // Every gap in the edge of a room must meet a gap in the room next to it, or the player would
    // come out inside rock. A gap leading nowhere is only allowed at the bottom (a pit) and in the
    // last room (the end of the game).
    const auto rooms = shippedWorld();
    for (size_t ii = 0; ii < rooms.size(); ++ii) {
        const Room& r = rooms[ii];
        SCOPED_TRACE(r.info.name);
        auto check = [&](int x, int y, int dx, int dy) {
            if (solidAt(r, x, y)) return;
            const auto other = roomAt(rooms, x + dx, y + dy);
            if (other) {
                EXPECT_FALSE(solidAt(rooms[*other], x + dx, y + dy))
                    << "opening at world tile " << x << "," << y << " runs into rock in " << rooms[*other].info.name;
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
    const auto rooms = shippedWorld();
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

namespace
{
    // A small LDtk project, as tools/rooms.py writes them: two rooms, each with a Tiles and an
    // Entities layer, and a title and theme.
    std::string ldtkRoom(const std::string& name, int worldX, int worldY, const std::string& csv, const std::string& entities)
    {
        return R"({"identifier":")" + name + R"(","worldX":)" + std::to_string(worldX) + R"(,"worldY":)" + std::to_string(worldY) +
               R"(,"pxWid":32,"pxHei":24,"fieldInstances":[{"__identifier":"title","__value":"Room )" + name +
               R"("},{"__identifier":"theme","__value":"snow"}],"layerInstances":[)" +
               R"({"__identifier":"Entities","entityInstances":[)" + entities + R"(]},)" +
               R"({"__identifier":"Tiles","__cWid":4,"__cHei":3,"intGridCsv":[)" + csv + "]}]}";
    }

    std::string entity(const std::string& kind, int x, int y, const std::string& fields = "")
    {
        return R"({"__identifier":")" + kind + R"(","__grid":[)" + std::to_string(x) + "," + std::to_string(y) +
               R"(],"fieldInstances":[)" + fields + "]}";
    }

    std::vector<LevelInfo> loadText(const std::string& text)
    {
        const auto path = std::filesystem::temp_directory_path() / "game_test_world.ldtk";
        std::ofstream(path) << text;
        auto rooms = loadWorld(path.string());
        std::filesystem::remove(path);
        return rooms;
    }
}

TEST(World, LoadsRoomsTilesAndEntitiesFromLDtk) {
    // A 4 x 3 room with rock round the edge, and one beside it and 1 tile higher. LDtk's y goes down.
    const std::string walls = "1,1,1,1, 2,0,0,1, 1,1,1,1";
    const std::string text = R"({"levels":[)" +
        ldtkRoom("a", 0, 8, walls, entity("Start", 1, 1) + "," + entity("Pickup", 2, 1, R"({"__identifier":"ability","__value":"climbing_pick"})")) + "," +
        ldtkRoom("b", 32, 0, walls, entity("Start", 2, 1) + "," + entity("Gem", 1, 1) + "," + entity("Checkpoint", 2, 1) + "," + entity("Bench", 1, 1)) + "]}";
    auto rooms = loadText(text);

    ASSERT_EQ(rooms.size(), 2u);
    EXPECT_EQ(rooms[0].name, "a");
    EXPECT_EQ(rooms[0].title, "Room a");
    EXPECT_EQ(rooms[0].theme, "snow");
    EXPECT_EQ(rooms[0].position, sf::Vector2i(0, 0));
    EXPECT_EQ(rooms[1].position, sf::Vector2i(4, 1));

    const Level& a = *rooms[0].level;
    EXPECT_TRUE(a.isSolid(0, 0));
    EXPECT_TRUE(a.isHookBlock(0, 1)); // value 2, in the left wall of the middle row
    EXPECT_FALSE(a.isSolid(1, 1));
    EXPECT_EQ(a.getStartPos(), sf::Vector2f(8.f, 8.f));
    ASSERT_EQ(a.pickups().size(), 1u);
    EXPECT_EQ(a.pickups()[0].ability, "climbing_pick");
    EXPECT_EQ(a.pickups()[0].position, sf::Vector2f(16.f, 8.f));
    const Level& b = *rooms[1].level;
    ASSERT_EQ(b.gems().size(), 1u);
    EXPECT_EQ(b.gems()[0], sf::Vector2f(8.f, 8.f));
    EXPECT_EQ(b.checkpoints().size(), 1u);
    ASSERT_EQ(b.benches().size(), 1u);
    EXPECT_EQ(b.benches()[0], sf::Vector2f(8.f, 8.f));
}

TEST(World, RoomsNeedAStartAndTheFileHasToBeReadable) {
    EXPECT_THROW(loadText(R"({"levels":[)" + ldtkRoom("a", 0, 0, "1,1,1,1,0,0,0,0,1,1,1,1", "") + "]}"), std::runtime_error);
    EXPECT_THROW(loadText("{not json"), std::runtime_error);
    EXPECT_THROW(loadText(R"({"levels":[{"identifier":"a"}]})"), std::runtime_error);
    EXPECT_THROW(loadWorld("no/such/world.ldtk"), std::runtime_error);
}

TEST(World, BenchesStandOnTheGround) {
    // A bench is two tiles wide: both have to be empty, with something to stand on under them.
    size_t benches = 0;
    for (const auto& room : loadWorld(config::assetPath("world.ldtk"))) {
        for (const sf::Vector2f& bench : room.level->benches()) {
            SCOPED_TRACE(room.name);
            const int x = static_cast<int>(bench.x / Level::TILE_SIZE), y = static_cast<int>(bench.y / Level::TILE_SIZE);
            for (int dx : {0, 1}) {
                EXPECT_FALSE(room.level->isSolid(x + dx, y));
                EXPECT_TRUE(room.level->isSolid(x + dx, y - 1) || room.level->isPlatform(x + dx, y - 1));
            }
            ++benches;
        }
    }
    EXPECT_GE(benches, 3u);
}

TEST(World, TheLDtkProjectDefinesEveryTileTheRoomsUse) {
    // Otherwise LDtk shows them as unknown, though the game would load them.
    std::ifstream file(config::assetPath("world.ldtk"));
    const auto project = nlohmann::json::parse(file);
    std::set<int> defined;
    for (const auto& layer : project["defs"]["layers"]) {
        for (const auto& value : layer["intGridValues"]) defined.insert(value["value"].get<int>());
    }
    for (const auto& level : project["levels"]) {
        for (const auto& layer : level["layerInstances"]) {
            for (const auto& value : layer["intGridCsv"]) {
                if (value.get<int>() != 0) EXPECT_TRUE(defined.count(value.get<int>())) << level["identifier"] << " uses tile " << value;
            }
        }
    }
}
