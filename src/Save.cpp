#include "Save.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    // Older saves named rooms after their text level files.
    std::string roomName(std::string name)
    {
        if (name.size() > 4 && name.compare(name.size() - 4, 4, ".txt") == 0) name.resize(name.size() - 4);
        return name;
    }

    std::string read(const std::string& path)
    {
        std::ifstream file(path);
        std::stringstream buffer;
        if (file) buffer << file.rdbuf();
        return buffer.str();
    }

    // Into a file next to it first, then swapped in whole: stopping halfway through (a crash, the
    // power going) leaves the old file as it was, never half of the new one.
    void write(const std::string& path, const std::string& text)
    {
        std::error_code error;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
        const std::string temporary = path + ".new";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (!file || !(file << text) || !file.flush()) return;
        }
        std::filesystem::rename(temporary, path, error);
        if (error) std::filesystem::remove(temporary, error);
    }

    // Calls `line` with the first word of every line and a stream of the rest.
    template <typename F>
    void forEachLine(const std::string& text, F line)
    {
        std::istringstream stream(text);
        for (std::string raw; std::getline(stream, raw);) {
            std::istringstream words(raw);
            std::string key;
            if (words >> key) line(key, words);
        }
    }
}

Settings Settings::parse(const std::string& text)
{
    Settings s;
    forEachLine(text, [&](const std::string& key, std::istringstream& words) {
        if (key == "window_scale") {
            int scale;
            if (words >> scale && scale >= 1 && scale <= 16) s.windowScale = scale;
        } else if (key == "fullscreen" || key == "screen_shake" || key == "show_timer") {
            int on;
            if (!(words >> on)) return;
            bool& flag = key == "fullscreen" ? s.fullscreen : key == "screen_shake" ? s.screenShake : s.showTimer;
            flag = on != 0;
        } else if (key == "skin") {
            words >> s.skin;
        } else if (key == "music_volume" || key == "sound_volume") {
            int level;
            if (words >> level && level >= 0 && level <= 10) (key == "music_volume" ? s.musicVolume : s.soundVolume) = level;
        } else if (key == "key" || key == "button") {
            // "key jump SPACE", "key left A LEFT", "button rope 2".
            std::string id;
            words >> id;
            for (size_t a = 0; a < controls::BINDABLE; ++a) {
                const auto action = static_cast<controls::Action>(a);
                if (controls::actionId(action) != id) continue;
                if (key == "button") {
                    int button;
                    if (words >> button && button >= 0 && button < 32) s.controls.buttons[a] = button;
                    return;
                }
                std::string first, second;
                words >> first >> second;
                const sf::Keyboard::Key one = controls::keyNamed(first), two = controls::keyNamed(second);
                if (one == sf::Keyboard::Unknown || controls::reserved(one) || (two != sf::Keyboard::Unknown && controls::reserved(two))) return;
                s.controls.keys[a] = {one, two};
            }
        } else if (key == "last_slot") {
            int slot;
            if (words >> slot && slot >= 0 && slot < SaveFiles::SLOTS) s.lastSlot = slot;
        }
    });
    return s;
}

std::string Settings::serialize() const
{
    std::ostringstream out;
    out << "window_scale " << windowScale << "\n";
    out << "fullscreen " << fullscreen << "\n";
    out << "screen_shake " << screenShake << "\n";
    out << "show_timer " << showTimer << "\n";
    out << "skin " << skin << "\n";
    out << "last_slot " << lastSlot << "\n";
    out << "music_volume " << musicVolume << "\n";
    out << "sound_volume " << soundVolume << "\n";
    // Only what was changed, so new defaults reach everyone else.
    const controls::Bindings defaults;
    for (size_t a = 0; a < controls::BINDABLE; ++a) {
        const std::string id = controls::actionId(static_cast<controls::Action>(a));
        const auto& keys = controls.keys[a];
        if (keys != defaults.keys[a]) {
            out << "key " << id << " " << controls::keyName(keys[0]);
            if (keys[1] != sf::Keyboard::Unknown) out << " " << controls::keyName(keys[1]);
            out << "\n";
        }
        if (controls.buttons[a] != controls::Bindings::PAD_DEFAULT) out << "button " << id << " " << controls.buttons[a] << "\n";
    }
    return out.str();
}

bool Progress::isEmpty() const
{
    return explored.empty() && bench.empty() && playTime == 0.f;
}

Progress Progress::parse(const std::string& text)
{
    Progress p;
    forEachLine(text, [&](const std::string& key, std::istringstream& words) {
        std::string room;
        int x, y;
        if (key == "bench" || key == "found_bench") {
            int number;
            if (!(words >> room >> number)) return;
            const std::string bench = room + " " + std::to_string(number);
            if (key == "bench") p.bench = bench;
            p.benches.insert(bench);
        } else if (key == "explored") {
            if (words >> room) p.explored.insert(roomName(room));
        } else if (key == "ability") {
            std::string ability;
            if (words >> ability) p.abilities.insert(ability);
        } else if (key == "gem") {
            if (words >> room >> x >> y) p.gems.insert(roomName(room) + " " + std::to_string(x) + " " + std::to_string(y));
        } else if (key == "talked") {
            std::string dialogue;
            if (words >> dialogue >> x) p.conversations.insert(dialogue + " " + std::to_string(x));
        } else if (key == "flag") {
            std::string flag;
            if (words >> flag) p.flags.insert(flag);
        } else if (key == "play_time") {
            words >> p.playTime;
        } else if (key == "deaths") {
            words >> p.deaths;
        } else if (key == "finished") {
            int on;
            if (words >> on) p.finished = on != 0;
        }
    });
    return p;
}

std::string Progress::serialize() const
{
    std::ostringstream out;
    if (!bench.empty()) out << "bench " << bench << "\n";
    for (const auto& b : benches) {
        if (b != bench) out << "found_bench " << b << "\n";
    }
    for (const auto& room : explored) out << "explored " << room << "\n";
    for (const auto& ability : abilities) out << "ability " << ability << "\n";
    for (const auto& gem : gems) out << "gem " << gem << "\n";
    for (const auto& conversation : conversations) out << "talked " << conversation << "\n";
    for (const auto& flag : flags) out << "flag " << flag << "\n";
    out << "play_time " << playTime << "\n";
    out << "deaths " << deaths << "\n";
    out << "finished " << finished << "\n";
    return out.str();
}

SaveFiles::SaveFiles(std::string folderPath) : folder(std::move(folderPath))
{
    // Before there were slots, everything was in save.txt: it becomes the settings and slot 1.
    const std::string old = path("save.txt");
    if (std::filesystem::exists(old) && !std::filesystem::exists(path("settings.txt"))) {
        const std::string text = read(old);
        write(path("settings.txt"), Settings::parse(text).serialize());
        const Progress progress = Progress::parse(text);
        if (!progress.isEmpty()) write(path("slot1.txt"), progress.serialize());
    }
}

std::string SaveFiles::defaultFolder()
{
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    return (home ? std::filesystem::path(home) / ".celeste_like_game" : std::filesystem::path(".")).string();
}

std::string SaveFiles::path(const std::string& file) const
{
    return (std::filesystem::path(folder) / file).string();
}

Settings SaveFiles::loadSettings() const
{
    return Settings::parse(read(path("settings.txt")));
}

void SaveFiles::saveSettings(const Settings& settings) const
{
    write(path("settings.txt"), settings.serialize());
}

Progress SaveFiles::loadSlot(int slot) const
{
    return Progress::parse(read(path("slot" + std::to_string(slot + 1) + ".txt")));
}

void SaveFiles::saveSlot(int slot, const Progress& progress) const
{
    write(path("slot" + std::to_string(slot + 1) + ".txt"), progress.serialize());
}

void SaveFiles::eraseSlot(int slot) const
{
    std::error_code error;
    std::filesystem::remove(path("slot" + std::to_string(slot + 1) + ".txt"), error);
}
