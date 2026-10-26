#include <gtest/gtest.h>
#include "src/Config.h"
#include "src/Game.h"
#include "src/Outfits.h"
#include "src/Physics.h"
#include <filesystem>
#include <fstream>
#include <functional>

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

    size_t roomNamed(const std::string& name)
    {
        size_t room = 0;
        while (room < world().size() && world()[room].name != name) ++room;
        return room;
    }

    // A world of one small room, made here: for what the world itself doesn't have (yet).
    std::vector<LevelInfo> oneRoom(const std::string& name, const std::vector<std::string>& rows,
                                   const std::function<void(Level&)>& add = {})
    {
        LevelInfo info;
        info.name = info.title = name;
        info.level = Level::fromRows(rows);
        if (add) add(*info.level);
        return {info};
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
    std::ofstream(saves + "/slot1.txt") << "bench lake 0\nexplored lake\nplay_time 5\n";
    {
        Game game(world(), {.headless = true, .saveFolder = saves});
        game.pressKey(sf::Keyboard::Enter);
        game.pressKey(sf::Keyboard::Enter);
        ASSERT_TRUE(game.isSitting());
        // Moving gets you up; back to the lake's bench, and sitting on it again saves.
        game.advance(0.15f, walking(1.f));
        EXPECT_FALSE(game.isSitting());
        game.advance(0.4f);
        game.advance(0.15f, walking(-1.f));
        game.advance(0.4f);
        game.pressKey(sf::Keyboard::W);
        ASSERT_TRUE(game.isSitting());
        EXPECT_EQ(game.state().bench, "lake 0");
        EXPECT_GT(game.state().playTime, 5.f);
    }
    Game again(world(), {.headless = true, .saveFolder = saves});
    again.pressKey(sf::Keyboard::Enter);
    again.pressKey(sf::Keyboard::Enter);
    EXPECT_TRUE(again.isSitting());
    EXPECT_EQ(again.roomName(), "lake");
}

TEST(GameFlow, TravelFromBenchToBench) {
    const std::string saves = freshSaves("game_flow_travel");
    std::ofstream(saves + "/slot1.txt") << "bench camp 0\nfound_bench lake 0\nexplored lake\nexplored camp\n";
    Game game(world(), {.headless = true, .saveFolder = saves});
    game.pressKey(sf::Keyboard::Enter);
    game.pressKey(sf::Keyboard::Enter);
    ASSERT_EQ(game.roomName(), "camp");
    ASSERT_TRUE(game.isSitting());

    game.pressKey(sf::Keyboard::M);
    EXPECT_EQ(game.screenName(), "map");
    game.pressKey(sf::Keyboard::Left); // the other bench
    game.pressKey(sf::Keyboard::Enter);
    EXPECT_EQ(game.screenName(), "playing");
    EXPECT_EQ(game.roomName(), "lake");
    EXPECT_TRUE(game.isSitting());
    EXPECT_EQ(game.state().bench, "lake 0");
}

TEST(GameFlow, OdoGivesYouTheRope) {
    Game game(world(), {.testing = true, .startRoom = roomNamed("old_road"), .headless = true, .saveFolder = freshSaves("game_flow_talk")});
    game.advance(0.5f);
    // A test room gives you everything: no rope, as when you first meet him, and no long rope (with
    // it, he's gone up to the station).
    game.pressKey(sf::Keyboard::Num1);
    game.pressKey(sf::Keyboard::Num2);
    ASSERT_FALSE(game.state().abilities.count("rope"));
    // Odo is along the road from where it starts.
    for (int i = 0; i < 40 && game.screenName() != "talking"; ++i) {
        game.advance(0.1f, walking(1.f));
        game.pressKey(sf::Keyboard::W);
    }
    ASSERT_EQ(game.screenName(), "talking");
    for (int i = 0; i < 300 && game.screenName() == "talking"; ++i) {
        game.advance(0.05f);
        game.pressKey(sf::Keyboard::Enter);
    }
    EXPECT_EQ(game.screenName(), "playing");
    EXPECT_TRUE(game.state().conversations.count("odo 0"));
    EXPECT_TRUE(game.state().abilities.count("rope"));
    EXPECT_TRUE(game.state().flags.count("met_odo"));
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

TEST(GameFlow, PullingALeverOpensTheGates) {
    const auto room = oneRoom("levers", {"##########", "#.......=#", "#.......=#", "#.......=#", "##########"},
                              [](Level& level) { level.setStart({4, 1}); level.addLever({2, 1}); });
    Game game(room, {.testing = true, .headless = true, .saveFolder = freshSaves("game_flow_lever")});
    game.advance(0.5f);
    // The lever is just left of the start.
    game.advance(0.3f, walking(-1.f));
    game.advance(0.3f);
    game.pressKey(sf::Keyboard::W);
    EXPECT_TRUE(game.state().flags.count("open_levers"));
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
    for (int ii = 0; ii < 6; ++ii) game.pressKey(sf::Keyboard::Down);
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

TEST(GameFlow, StayingUnderWaterTooLongDrowns) {
    const auto pool = oneRoom("pool", {"##########", "#........#", "#.S......#", "#wwwwwwww#", "#wwwwwwww#", "#wwwwwwww#", "#wwwwwwww#",
                                       "##########"});
    Game game(pool, {.testing = true, .headless = true, .saveFolder = freshSaves("game_flow_drown")});
    // Floating at the surface, you breathe.
    game.advance(physics().airTime + 2.f);
    EXPECT_EQ(game.state().deaths, 0);
    // Held under, you don't, for long.
    PlayerInput down;
    down.moveY = -1.f;
    game.advance(physics().airTime * 0.5f, down);
    game.screenshot().saveToFile(pictures("holding_breath.png"));
    EXPECT_EQ(game.state().deaths, 0);
    game.advance(physics().airTime * 0.5f + 0.5f, down);
    EXPECT_EQ(game.state().deaths, 1);
}

TEST(GameFlow, TheRoomEditorPlaysARoomWhereItsMade) {
    // The room editor drives the game frame by frame, from wherever you say, its mouse in the picture's pixels.
    const auto room = oneRoom("editing", {"####################", "#..................#", "#..................#", "#.S................#", "####################"});
    Game game(room, {.testing = true, .headless = true, .saveFolder = freshSaves("game_flow_editor"), .startAt = sf::Vector2f(80.f, 8.f)});
    PlayerInput in;
    game.play(1.f / 60.f, in);
    EXPECT_FLOAT_EQ(game.playerPosition().x, 80.f); // not the Start
    in.moveX = 1.f;
    in.aim = {300.f, 20.f};
    for (int i = 0; i < 30; ++i) {
        const sf::Texture& picture = game.play(1.f / 60.f, in);
        EXPECT_EQ(picture.getSize(), sf::Vector2u(config::GAME_WIDTH, config::GAME_HEIGHT));
    }
    EXPECT_GT(game.playerPosition().x, 85.f);
}

TEST(GameFlow, MagsSellsOutfitsForGems) {
    // Talking to Mags opens her stall; gems buy what's on it, and the diving suit holds your breath longer.
    const auto room = oneRoom("stall", {"####################", "#..................#", "#.S..N.............#", "####################"});
    room[0].level->addNpc("mags", "mags", {5, 1});
    std::vector<LevelInfo> rooms = room;
    for (int g = 0; g < 12; ++g) rooms[0].level->addGem({8 + g % 10, 1});
    Game game(rooms, {.testing = true, .headless = true, .saveFolder = freshSaves("game_flow_stall")});
    PlayerInput right;
    right.moveX = 1.f;
    game.advance(2.f, right); // over the gems, to the wall
    EXPECT_GE(game.state().gems.size(), 10u);
    PlayerInput left;
    left.moveX = -1.f;
    for (int i = 0; i < 60 && game.screenName() != "talking"; ++i) {
        game.advance(0.05f, left);
        game.pressKey(sf::Keyboard::W);
    }
    ASSERT_EQ(game.screenName(), "talking");
    for (int i = 0; i < 300 && game.screenName() == "talking"; ++i) {
        game.advance(0.05f);
        game.pressKey(sf::Keyboard::Enter);
    }
    ASSERT_EQ(game.screenName(), "shop");
    // Down to the diving suit and buy it.
    const auto& all = outfits::all();
    const auto diver = std::find_if(all.begin(), all.end(), [](const outfits::Outfit& o) { return o.id == outfits::DIVER; });
    ASSERT_NE(diver, all.end());
    for (auto it = all.begin(); it != diver; ++it) game.pressKey(sf::Keyboard::Down);
    game.pressKey(sf::Keyboard::Enter);
    EXPECT_EQ(game.state().outfit, outfits::DIVER);
    EXPECT_EQ(game.state().spent, diver->price);
    EXPECT_TRUE(game.state().outfits.count(outfits::DIVER));
}

TEST(GameFlow, TheCartTakesYouAlongAndBringsYouBackInItIfYouCrash) {
    // Walk into the cart and it rolls off; into the wall at the end it's wrecked, and you're back in
    // it where it set off.
    const auto room = oneRoom("line", {"##############################", "#............................#", "#............................#",
                                       "#.S.K........................#", "#tttttttttttttttttttttttttttt#", "##############################"});
    std::vector<LevelInfo> rooms = room;
    Game game(rooms, {.testing = true, .headless = true, .saveFolder = freshSaves("game_flow_cart")});
    game.advance(0.3f);
    PlayerInput right;
    right.moveX = 1.f;
    game.advance(0.5f, right); // into it
    const float x = game.playerPosition().x;
    game.advance(0.5f);         // it goes on by itself
    EXPECT_GT(game.playerPosition().x, x + 40.f);
    const int deaths = game.state().deaths;
    game.advance(3.f);          // into the wall
    EXPECT_GT(game.state().deaths, deaths);
    game.advance(1.f);          // back in it, rolling again
    EXPECT_LT(game.playerPosition().x, 200.f);
}
