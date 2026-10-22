#include <gtest/gtest.h>
#include "src/Save.h"
#include <filesystem>

TEST(SaveData, RoundTripsThroughText) {
    SaveData data;
    data.settings = {.windowScale = 3, .fullscreen = true, .screenShake = false, .showTimer = false, .skin = "robot"};
    data.continueLevel = "level4.txt";
    data.addResult("level1.txt", 42.5f, 3);
    data.addResult("level2.txt", 61.25f, 0);
    data.explored = {"level1.txt", "level2.txt"};
    data.abilities = {"climbing_pick"};
    data.gems = {"level3.txt 12 5", "level1.txt 70 9"};

    SaveData loaded = SaveData::parse(data.serialize());

    EXPECT_EQ(loaded.settings.windowScale, 3);
    EXPECT_TRUE(loaded.settings.fullscreen);
    EXPECT_FALSE(loaded.settings.screenShake);
    EXPECT_FALSE(loaded.settings.showTimer);
    EXPECT_EQ(loaded.settings.skin, "robot");
    EXPECT_EQ(loaded.continueLevel, "level4.txt");
    ASSERT_NE(loaded.record("level1.txt"), nullptr);
    EXPECT_FLOAT_EQ(loaded.record("level1.txt")->bestTime, 42.5f);
    EXPECT_EQ(loaded.record("level1.txt")->fewestDeaths, 3);
    ASSERT_NE(loaded.record("level2.txt"), nullptr);
    EXPECT_FLOAT_EQ(loaded.record("level2.txt")->bestTime, 61.25f);
    EXPECT_EQ(loaded.record("level3.txt"), nullptr);
    EXPECT_EQ(loaded.explored, (std::set<std::string>{"level1.txt", "level2.txt"}));
    EXPECT_EQ(loaded.abilities, std::set<std::string>{"climbing_pick"});
    EXPECT_EQ(loaded.gems, data.gems);
}

TEST(SaveData, KeepsBestTimeAndFewestDeathsSeparately) {
    SaveData data;
    data.addResult("a.txt", 30.f, 5);
    data.addResult("a.txt", 40.f, 1); // slower, but fewer deaths
    data.addResult("a.txt", 35.f, 9); // neither

    ASSERT_NE(data.record("a.txt"), nullptr);
    EXPECT_FLOAT_EQ(data.record("a.txt")->bestTime, 30.f);
    EXPECT_EQ(data.record("a.txt")->fewestDeaths, 1);
}

TEST(SaveData, SkipsBrokenLines) {
    SaveData data = SaveData::parse(
        "nonsense here\n"
        "window_scale lots\n"
        "window_scale 999\n"
        "record half.txt 12\n"
        "\n"
        "fullscreen 1\n");

    EXPECT_EQ(data.settings.windowScale, Settings{}.windowScale);
    EXPECT_TRUE(data.settings.fullscreen);
    EXPECT_TRUE(data.records.empty());
}

TEST(SaveData, SavesAndLoadsAFile) {
    const auto folder = std::filesystem::temp_directory_path() / "game_test_save";
    std::filesystem::remove_all(folder);
    const std::string path = (folder / "save.txt").string();

    EXPECT_EQ(SaveData::load(path).settings.windowScale, Settings{}.windowScale); // missing file: defaults

    SaveData data;
    data.settings.windowScale = 2;
    data.save(path); // creates the folder
    EXPECT_EQ(SaveData::load(path).settings.windowScale, 2);
    std::filesystem::remove_all(folder);
}
