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

    // Where the tests leave pictures to look at: build/screenshots.
    std::string pictures(const std::string& file)
    {
        const auto folder = std::filesystem::path(config::assetPath("..")) / "build" / "screenshots";
        std::filesystem::create_directories(folder);
        return (folder / file).string();
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
    EXPECT_TRUE(game.state().abilities.empty()); // not even the rope: it's found further on
}

TEST(GameFlow, SittingOnABenchSavesAndContinueStartsThere) {
    const std::string saves = freshSaves("game_flow_bench");
    std::ofstream(saves + "/slot1.txt") << "bench level1 0\nexplored level1\nplay_time 5\n";
    {
        Game game(world(), {.headless = true, .saveFolder = saves});
        game.pressKey(sf::Keyboard::Enter);
        game.pressKey(sf::Keyboard::Enter);
        ASSERT_TRUE(game.isSitting());
        // Moving gets you up; back to The Cave's bench, and sitting on it again saves.
        game.advance(0.15f, walking(1.f));
        EXPECT_FALSE(game.isSitting());
        game.advance(0.4f);
        game.advance(0.15f, walking(-1.f));
        game.advance(0.4f);
        game.pressKey(sf::Keyboard::W);
        ASSERT_TRUE(game.isSitting());
        EXPECT_EQ(game.state().bench, "level1 0");
        EXPECT_GT(game.state().playTime, 5.f);
    }
    Game again(world(), {.headless = true, .saveFolder = saves});
    again.pressKey(sf::Keyboard::Enter);
    again.pressKey(sf::Keyboard::Enter);
    EXPECT_TRUE(again.isSitting());
    EXPECT_EQ(again.roomName(), "level1");
    EXPECT_TRUE(again.state().abilities.count("rope")); // a save from before the rope was found
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
    const std::string saves = freshSaves("game_flow_talk");
    std::ofstream(saves + "/slot1.txt") << "bench level1 0\nexplored level1\nability rope\n";
    Game game(world(), {.headless = true, .saveFolder = saves});
    game.pressKey(sf::Keyboard::Enter);
    game.pressKey(sf::Keyboard::Enter);
    // Wren stands a few steps right of The Cave's bench.
    game.advance(0.6f, walking(1.f));
    game.advance(0.4f);
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
    std::set<std::string> done;
    for (size_t room = 0; room < world().size(); ++room) {
        if (!done.insert(world()[room].theme).second) continue;
        Game game(world(), {.testing = true, .startRoom = room, .headless = true, .saveFolder = freshSaves("game_flow_pictures")});
        game.advance(1.f);
        const sf::Image picture = game.screenshot();
        picture.saveToFile(pictures(world()[room].theme + ".png"));
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
    game.advance(0.3f, walking(-1.f));
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

TEST(GameFlow, ChangingAKeyInTheControls) {
    const std::string saves = freshSaves("game_flow_controls");
    Game game(world(), {.headless = true, .saveFolder = saves});
    game.pressKey(sf::Keyboard::Down);
    game.pressKey(sf::Keyboard::Enter); // OPTIONS
    for (int ii = 0; ii < 6; ++ii) game.pressKey(sf::Keyboard::Down); // (no outfits out of a game)
    game.pressKey(sf::Keyboard::Enter); // CONTROLS
    ASSERT_EQ(game.screenName(), "controls");
    for (int ii = 0; ii < 4; ++ii) game.pressKey(sf::Keyboard::Down);
    game.pressKey(sf::Keyboard::Enter); // JUMP: waiting for a key
    game.pressKey(sf::Keyboard::Enter); // ...not that one, it works the menus
    game.pressKey(sf::Keyboard::C);
    game.screenshot().saveToFile(pictures("controls.png"));

    std::ifstream file(saves + "/settings.txt");
    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(text.find("key jump C"), std::string::npos);
    // And Escape while waiting keeps the old key.
    game.pressKey(sf::Keyboard::Enter);
    game.pressKey(sf::Keyboard::Escape);
    EXPECT_EQ(game.screenName(), "controls");
    game.pressKey(sf::Keyboard::Escape);
    EXPECT_EQ(game.screenName(), "options");
}

TEST(GameFlow, AGamepadWorksTheMenusAndThePause) {
    Game game(world(), {.headless = true, .saveFolder = freshSaves("game_flow_pad")});
    game.pressButton(0); // A: PLAY
    EXPECT_EQ(game.screenName(), "slots");
    game.pressButton(1); // B: back
    EXPECT_EQ(game.screenName(), "title");
    game.pressButton(0);
    game.pressButton(0); // slot 1
    ASSERT_EQ(game.screenName(), "playing");
    game.pressButton(7); // Start
    EXPECT_EQ(game.screenName(), "paused");
    game.screenshot().saveToFile(pictures("paused_pad.png"));
    game.pressButton(7);
    EXPECT_EQ(game.screenName(), "playing");
    game.pressButton(3); // Y: the map
    EXPECT_EQ(game.screenName(), "map");
    game.pressButton(1);
    EXPECT_EQ(game.screenName(), "playing");
}

TEST(GameFlow, TuckSellsOutfitsForGems) {
    const std::string saves = freshSaves("game_flow_shop");
    std::ofstream(saves + "/slot1.txt") << "bench market 0\nexplored market\nability rope\nspent -4\n"; // 4 gems to spend
    Game game(world(), {.headless = true, .saveFolder = saves});
    game.pressKey(sf::Keyboard::Enter);
    game.pressKey(sf::Keyboard::Enter);
    ASSERT_EQ(game.roomName(), "market");
    // Along from the bench to Tuck (W by the bench would sit back down), and talk.
    game.advance(0.6f, walking(1.f));
    for (int i = 0; i < 40 && game.screenName() != "talking"; ++i) {
        game.advance(0.1f, walking(1.f));
        game.pressKey(sf::Keyboard::W);
    }
    ASSERT_EQ(game.screenName(), "talking");
    for (int i = 0; i < 200 && game.screenName() == "talking"; ++i) {
        game.advance(0.05f);
        game.pressKey(sf::Keyboard::Enter); // through the lines, and "Let's see what you've got"
    }
    ASSERT_EQ(game.screenName(), "shop");
    // The classic look first, then the kid, 3 gems.
    game.pressKey(sf::Keyboard::Down);
    game.pressKey(sf::Keyboard::Enter);
    EXPECT_TRUE(game.state().outfits.count("kid"));
    EXPECT_EQ(game.state().wearing, "kid");
    EXPECT_EQ(game.state().spent, -1);
    // The frog is 4: one gem short.
    game.pressKey(sf::Keyboard::Down);
    game.pressKey(sf::Keyboard::Enter);
    EXPECT_FALSE(game.state().outfits.count("frog"));
}
