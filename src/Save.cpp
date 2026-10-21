#include "Save.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

void SaveData::addResult(const std::string& level, float time, int deaths)
{
    auto [it, isNew] = records.try_emplace(level, LevelRecord{time, deaths});
    if (isNew) return;
    it->second.bestTime = std::min(it->second.bestTime, time);
    it->second.fewestDeaths = std::min(it->second.fewestDeaths, deaths);
}

const LevelRecord* SaveData::record(const std::string& level) const
{
    auto it = records.find(level);
    return it == records.end() ? nullptr : &it->second;
}

SaveData SaveData::parse(const std::string& text)
{
    SaveData data;
    std::istringstream stream(text);
    for (std::string line; std::getline(stream, line);) {
        std::istringstream words(line);
        std::string key;
        if (!(words >> key)) continue;

        if (key == "window_scale") {
            int scale;
            if (words >> scale && scale >= 1 && scale <= 16) data.settings.windowScale = scale;
        } else if (key == "fullscreen" || key == "screen_shake" || key == "show_timer") {
            int on;
            if (!(words >> on)) continue;
            bool& flag = key == "fullscreen" ? data.settings.fullscreen
                       : key == "screen_shake" ? data.settings.screenShake
                       : data.settings.showTimer;
            flag = on != 0;
        } else if (key == "continue") {
            words >> data.continueLevel;
        } else if (key == "record") {
            std::string level;
            LevelRecord record;
            if (words >> level >> record.bestTime >> record.fewestDeaths) data.records[level] = record;
        }
    }
    return data;
}

std::string SaveData::serialize() const
{
    std::ostringstream out;
    out << "window_scale " << settings.windowScale << "\n";
    out << "fullscreen " << settings.fullscreen << "\n";
    out << "screen_shake " << settings.screenShake << "\n";
    out << "show_timer " << settings.showTimer << "\n";
    if (!continueLevel.empty()) out << "continue " << continueLevel << "\n";
    for (const auto& [level, record] : records) {
        out << "record " << level << " " << record.bestTime << " " << record.fewestDeaths << "\n";
    }
    return out.str();
}

SaveData SaveData::load(const std::string& path)
{
    std::ifstream file(path);
    if (!file) return {};
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str());
}

void SaveData::save(const std::string& path) const
{
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
    std::ofstream file(path);
    if (file) file << serialize();
}

std::string SaveData::defaultPath()
{
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    const std::filesystem::path folder = home ? std::filesystem::path(home) / ".celeste_like_game" : ".";
    return (folder / "save.txt").string();
}
