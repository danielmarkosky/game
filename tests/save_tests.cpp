#include <gtest/gtest.h>
#include "src/Save.h"
#include <filesystem>
#include <fstream>

namespace
{
    std::filesystem::path freshFolder(const std::string& name)
    {
        const auto folder = std::filesystem::temp_directory_path() / name;
        std::filesystem::remove_all(folder);
        return folder;
    }
}

TEST(Settings, RoundTripThroughText) {
    const Settings s{.windowScale = 3, .fullscreen = true, .screenShake = false, .showTimer = false, .skin = "robot", .lastSlot = 2};
    const Settings loaded = Settings::parse(s.serialize());
    EXPECT_EQ(loaded.windowScale, 3);
    EXPECT_TRUE(loaded.fullscreen);
    EXPECT_FALSE(loaded.screenShake);
    EXPECT_FALSE(loaded.showTimer);
    EXPECT_EQ(loaded.skin, "robot");
    EXPECT_EQ(loaded.lastSlot, 2);
}

TEST(Settings, SkipBrokenLines) {
    const Settings s = Settings::parse("nonsense here\nwindow_scale lots\nwindow_scale 999\nlast_slot 7\n\nfullscreen 1\n");
    EXPECT_EQ(s.windowScale, Settings{}.windowScale);
    EXPECT_EQ(s.lastSlot, 0);
    EXPECT_TRUE(s.fullscreen);
}

TEST(Progress, RoundTripsThroughText) {
    Progress p;
    p.bench = "crossroads 0";
    p.benches = {"crossroads 0", "level1 0"};
    p.explored = {"level1", "level2"};
    p.abilities = {"climbing_pick"};
    p.gems = {"level3 12 5", "level1 70 9"};
    p.conversations = {"wren 0", "wren 2"};
    p.playTime = 123.5f;
    p.deaths = 7;
    p.finished = true;

    const Progress loaded = Progress::parse(p.serialize());
    EXPECT_EQ(loaded.bench, p.bench);
    EXPECT_EQ(loaded.benches, p.benches);
    EXPECT_EQ(loaded.explored, p.explored);
    EXPECT_EQ(loaded.abilities, p.abilities);
    EXPECT_EQ(loaded.gems, p.gems);
    EXPECT_EQ(loaded.conversations, p.conversations);
    EXPECT_FLOAT_EQ(loaded.playTime, 123.5f);
    EXPECT_EQ(loaded.deaths, 7);
    EXPECT_TRUE(loaded.finished);
    EXPECT_FALSE(loaded.isEmpty());
    EXPECT_TRUE(Progress::parse("").isEmpty());
}

TEST(Progress, ReadsRoomNamesFromOlderSaves) {
    // Before the LDtk project, rooms were named after their text files.
    const Progress loaded = Progress::parse("continue level4.txt\nrecord level1.txt 42.5 3\nexplored level2.txt\ngem level3.txt 12 5\n");
    EXPECT_EQ(loaded.explored, std::set<std::string>{"level2"});
    EXPECT_EQ(loaded.gems, std::set<std::string>{"level3 12 5"});
}

TEST(SaveFiles, KeepSettingsAndEachSlotApart) {
    const auto folder = freshFolder("game_test_slots");
    SaveFiles files(folder.string());
    EXPECT_EQ(files.loadSettings().windowScale, Settings{}.windowScale); // missing files: defaults
    EXPECT_TRUE(files.loadSlot(1).isEmpty());

    Settings s;
    s.windowScale = 2;
    files.saveSettings(s); // creates the folder
    Progress p;
    p.deaths = 3;
    p.explored = {"level1"};
    files.saveSlot(1, p);

    EXPECT_EQ(files.loadSettings().windowScale, 2);
    EXPECT_EQ(files.loadSlot(1).deaths, 3);
    EXPECT_TRUE(files.loadSlot(0).isEmpty());
    files.eraseSlot(1);
    EXPECT_TRUE(files.loadSlot(1).isEmpty());
    std::filesystem::remove_all(folder);
}

TEST(SaveFiles, TurnTheOldSaveIntoSettingsAndTheFirstSlot) {
    const auto folder = freshFolder("game_test_old_save");
    std::filesystem::create_directories(folder);
    std::ofstream(folder / "save.txt") << "window_scale 3\nskin fox\ncontinue level4.txt\nexplored level1.txt\nability climbing_pick\n";

    SaveFiles files(folder.string());
    EXPECT_EQ(files.loadSettings().windowScale, 3);
    EXPECT_EQ(files.loadSettings().skin, "fox");
    EXPECT_EQ(files.loadSlot(0).abilities, std::set<std::string>{"climbing_pick"});
    EXPECT_EQ(files.loadSlot(0).explored, std::set<std::string>{"level1"});
    std::filesystem::remove_all(folder);
}
