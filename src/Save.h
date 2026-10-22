#pragma once

#include <set>
#include <string>

// Shared by every save slot.
struct Settings
{
    int windowScale = 4;     // window is the game's resolution times this, when not fullscreen
    bool fullscreen = false;
    bool screenShake = true;
    bool showTimer = true;   // the play time and death counter in the corner
    std::string skin = "classic"; // one of Player::SKINS
    int lastSlot = 0;        // the slot played last: its room drifts by behind the title screen
    int musicVolume = 6;     // 0 to 10
    int soundVolume = 8;

    // Unknown or broken lines are skipped, so an old or hand-edited file never stops the game.
    static Settings parse(const std::string& text);
    std::string serialize() const;
};

// One playthrough. Rooms are named by their identifier in the LDtk project, and a bench by its
// room and its number in the room, counting from 0: "crossroads 0".
struct Progress
{
    std::string bench;                    // sat on last: Continue starts there. Empty: at the start
    std::set<std::string> benches;        // sat on: the places to travel between
    std::set<std::string> explored;       // the rooms shown on the map
    std::set<std::string> abilities;      // picked up, e.g. "climbing_pick"
    std::set<std::string> gems;           // found, as "<room> <tile x> <tile y>"
    std::set<std::string> conversations; // had, as "<dialogue> <number>", counting from 0
    std::set<std::string> flags;          // set in conversations: what's happened in the story
    float playTime = 0.f;                 // seconds
    int deaths = 0;
    bool finished = false;                // got out at the end at least once

    bool isEmpty() const; // never played

    static Progress parse(const std::string& text);
    std::string serialize() const;
};

// The files in a folder (~/.celeste_like_game/ for the game): settings.txt, and slot1.txt to
// slot3.txt. A missing or unreadable file gives the defaults, and failing to write one is ignored:
// losing the save is better than crashing the game.
class SaveFiles
{
public:
    static constexpr int SLOTS = 3;

    explicit SaveFiles(std::string folder);
    static std::string defaultFolder();

    Settings loadSettings() const;
    void saveSettings(const Settings& settings) const;
    // Slots are numbered from 0.
    Progress loadSlot(int slot) const;
    void saveSlot(int slot, const Progress& progress) const;
    void eraseSlot(int slot) const;

private:
    std::string path(const std::string& file) const;
    std::string folder;
};
