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
    const Settings s{.windowScale = 3, .fullscreen = true, .screenShake = false, .showTimer = false, .lastSlot = 2,
                     .musicVolume = 3, .soundVolume = 10};
    const Settings loaded = Settings::parse(s.serialize());
    EXPECT_EQ(loaded.windowScale, 3);
    EXPECT_TRUE(loaded.fullscreen);
    EXPECT_FALSE(loaded.screenShake);
    EXPECT_FALSE(loaded.showTimer);
    EXPECT_EQ(loaded.lastSlot, 2);
    EXPECT_EQ(loaded.musicVolume, 3);
    EXPECT_EQ(loaded.soundVolume, 10);
}

TEST(Settings, SkipBrokenLines) {
    const Settings s = Settings::parse("nonsense here\nwindow_scale lots\nwindow_scale 999\nlast_slot 7\n\nfullscreen 1\n");
    EXPECT_EQ(s.windowScale, Settings{}.windowScale);
    EXPECT_EQ(s.lastSlot, 0);
    EXPECT_TRUE(s.fullscreen);
}

TEST(Progress, RoundTripsThroughText) {
    Progress p;
    p.bench = "lake 0";
    p.benches = {"lake 0", "level1 0"};
    p.explored = {"level1", "level2"};
    p.abilities = {"climbing_pick"};
    p.gems = {"level3 12 5", "level1 70 9"};
    p.conversations = {"odo 0", "odo 2"};
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

TEST(Settings, KeepOnlyTheControlsThatWereChanged) {
    Settings s;
    EXPECT_EQ(s.serialize().find("key "), std::string::npos);
    s.controls.bindKey(controls::Action::Jump, sf::Keyboard::C);
    s.controls.bindKey(controls::Action::Left, sf::Keyboard::Comma);
    s.controls.bindButton(controls::Action::Rope, 5, controls::PadLayout::Xbox);
    const Settings loaded = Settings::parse(s.serialize());
    EXPECT_EQ(loaded.controls, s.controls);
    EXPECT_NE(s.serialize().find("key jump C\n"), std::string::npos);
    EXPECT_NE(s.serialize().find("button rope 5\n"), std::string::npos);
    // Nonsense, or the keys that work the menus, leave the defaults.
    const Settings broken = Settings::parse("key jump ESCAPE\nkey nothing A\nkey up\nbutton jump many\n");
    EXPECT_EQ(broken.controls, controls::Bindings{});
}

TEST(SaveFiles, SavingReplacesTheFileWhole) {
    const auto folder = freshFolder("game_test_safe_save");
    SaveFiles files(folder.string());
    Progress p;
    p.deaths = 1;
    files.saveSlot(0, p);
    p.deaths = 2;
    files.saveSlot(0, p);
    EXPECT_EQ(files.loadSlot(0).deaths, 2);
    // Nothing left lying around from the swap.
    for (const auto& entry : std::filesystem::directory_iterator(folder)) EXPECT_EQ(entry.path().extension(), ".txt") << entry.path();
    std::filesystem::remove_all(folder);
}
