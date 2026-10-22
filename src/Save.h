#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>

struct Settings
{
    int windowScale = 4;     // window is the game's resolution times this, when not fullscreen
    bool fullscreen = false;
    bool screenShake = true;
    bool showTimer = true;   // the time and death counter in the corner
    std::string skin = "classic"; // one of Player::SKINS
};

// Best result on one level. Time and deaths are kept separately, like in Celeste:
// the fastest run and the run with the fewest deaths don't have to be the same one.
struct LevelRecord
{
    float bestTime = 0.f;
    int fewestDeaths = 0;
};

// Everything that's kept between sessions, stored as a small text file.
struct SaveData
{
    Settings settings;
    std::string continueLevel;                   // file name of the last level reached, empty if none
    std::map<std::string, LevelRecord> records;  // by level file name
    std::set<std::string> explored;              // file names of the rooms shown on the map
    std::set<std::string> abilities;             // picked up, e.g. "climbing_pick"
    std::set<std::string> gems;                  // found, as "<level file> <tile x> <tile y>"

    // Keeps the better time and the fewer deaths separately.
    void addResult(const std::string& level, float time, int deaths);
    const LevelRecord* record(const std::string& level) const;

    // Unknown or broken lines are skipped, so an old or hand-edited file never stops the game.
    static SaveData parse(const std::string& text);
    std::string serialize() const;

    // A missing or unreadable file gives the defaults. Saving failures are ignored: losing
    // the save is better than crashing the game.
    static SaveData load(const std::string& path);
    void save(const std::string& path) const;

    // In the user's home folder: ~/.celeste_like_game/save.txt
    static std::string defaultPath();
};
