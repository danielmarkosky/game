#include <gtest/gtest.h>
#include "src/Config.h"
#include "src/Game.h"
#include <filesystem>
#include <fstream>

// The whole game, headless: no window, saving into a folder of its own.
namespace
{
    std::string freshSaves(const std::string& name)
    {
        const auto folder = std::filesystem::temp_directory_path() / name;
        std::filesystem::remove_all(folder);
        std::filesystem::create_directories(folder);
        return folder.string();
    }

    PlayerInput walking(float x)
    {
        PlayerInput in;
        in.moveX = x;
        return in;
    }

    std::vector<LevelInfo>& world()
    {
        static std::vector<LevelInfo> rooms = loadWorld(config::assetPath("world.ldtk"));
        return rooms;
    }
}

TEST(GameFlow, AnEmptySlotStartsAtTheBeginning) {
    Game game(world(), {.headless = true, .saveFolder = freshSaves("game_flow_new")});
    EXPECT_EQ(game.screenName(), "title");
    game.pressKey(sf::Keyboard::Enter); // PLAY
    EXPECT_EQ(game.screenName(), "slots");
    game.pressKey(sf::Keyboard::Enter); // slot 1
    EXPECT_EQ(game.screenName(), "playing");
    EXPECT_EQ(game.roomName(), world().front().name);
    EXPECT_FALSE(game.isSitting());
}

TEST(GameFlow, SittingOnABenchSavesAndContinueStartsThere) {
    const std::string saves = freshSaves("game_flow_bench");
    {
        Game game(world(), {.headless = true, .saveFolder = saves});
        game.pressKey(sf::Keyboard::Enter);
        game.pressKey(sf::Keyboard::Enter);
        // The Cave's bench is just left of the start.
        game.advance(0.3f, walking(-1.f));
        game.advance(0.3f);
        game.pressKey(sf::Keyboard::W);
        ASSERT_TRUE(game.isSitting());
        EXPECT_EQ(game.state().bench, world().front().name + " 0");
        // Moving gets you up.
        game.advance(0.2f, walking(1.f));
        EXPECT_FALSE(game.isSitting());
    }
    Game again(world(), {.headless = true, .saveFolder = saves});
    again.pressKey(sf::Keyboard::Enter);
    again.pressKey(sf::Keyboard::Enter);
    EXPECT_TRUE(again.isSitting());
    EXPECT_EQ(again.state().bench, world().front().name + " 0");
}

TEST(GameFlow, TravelFromBenchToBench) {
    const std::string saves = freshSaves("game_flow_travel");
    std::ofstream(saves + "/slot1.txt") << "bench crossroads 0\nfound_bench level1 0\nexplored level1\nexplored crossroads\n";
    Game game(world(), {.headless = true, .saveFolder = saves});
    game.pressKey(sf::Keyboard::Enter);
    game.pressKey(sf::Keyboard::Enter);
    ASSERT_EQ(game.roomName(), "crossroads");
    ASSERT_TRUE(game.isSitting());

    game.pressKey(sf::Keyboard::M);
    EXPECT_EQ(game.screenName(), "map");
    game.pressKey(sf::Keyboard::Left); // the other bench
    game.pressKey(sf::Keyboard::Enter);
    EXPECT_EQ(game.screenName(), "playing");
    EXPECT_EQ(game.roomName(), "level1");
    EXPECT_TRUE(game.isSitting());
    EXPECT_EQ(game.state().bench, "level1 0");
}

TEST(GameFlow, TalkingToWren) {
    Game game(world(), {.headless = true, .saveFolder = freshSaves("game_flow_talk")});
    game.pressKey(sf::Keyboard::Enter);
    game.pressKey(sf::Keyboard::Enter);
    // Wren stands a few steps right of the start.
    game.advance(0.25f, walking(1.f));
    game.advance(0.3f);
    game.pressKey(sf::Keyboard::W);
    ASSERT_EQ(game.screenName(), "talking");
    for (int i = 0; i < 200 && game.screenName() == "talking"; ++i) {
        game.advance(0.05f);
        game.pressKey(sf::Keyboard::Enter);
    }
    EXPECT_EQ(game.screenName(), "playing");
    EXPECT_TRUE(game.state().conversations.count("wren 0"));
}

TEST(GameFlow, EveryThemeDraws) {
    // A picture of the first room of each theme: never all one colour. Kept in the build folder,
    // to look at.
    const auto pictures = std::filesystem::path(config::assetPath("..")) / "build" / "screenshots";
    std::filesystem::create_directories(pictures);
    std::set<std::string> done;
    for (size_t room = 0; room < world().size(); ++room) {
        if (!done.insert(world()[room].theme).second) continue;
        Game game(world(), {.testing = true, .startRoom = room, .headless = true, .saveFolder = freshSaves("game_flow_pictures")});
        game.advance(1.f);
        const sf::Image picture = game.screenshot();
        picture.saveToFile((pictures / (world()[room].theme + ".png")).string());
        std::set<sf::Uint32> colours;
        for (unsigned y = 0; y < picture.getSize().y; y += 7) {
            for (unsigned x = 0; x < picture.getSize().x; x += 7) colours.insert(picture.getPixel(x, y).toInteger());
        }
        EXPECT_GT(colours.size(), 20u) << world()[room].theme;
    }
}

TEST(GameFlow, PullingTheSummitLeverOpensTheDrop) {
    size_t summit = 0;
    while (world()[summit].name != "level5") ++summit;
    Game game(world(), {.testing = true, .startRoom = summit, .headless = true, .saveFolder = freshSaves("game_flow_lever")});
    game.advance(0.5f);
    // The lever is just left of the start.
    game.advance(0.2f, walking(-1.f));
    game.advance(0.3f);
    game.pressKey(sf::Keyboard::W);
    EXPECT_TRUE(game.state().flags.count("open_level5"));
}

TEST(GameFlow, GettingOutIsTheEnd) {
    const auto path = std::filesystem::temp_directory_path() / "game_flow_end.txt";
    std::ofstream(path) << "#######\n#.....\n#.....\n#S....\n#######\n";
    std::vector<LevelInfo> one = {loadSingleLevel(path.string())};
    Game game(one, {.testing = true, .headless = true, .saveFolder = freshSaves("game_flow_end")});
    game.advance(2.f, walking(1.f));
    EXPECT_EQ(game.screenName(), "complete");
    std::filesystem::remove(path);
}
