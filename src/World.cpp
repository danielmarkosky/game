#include "World.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using nlohmann::json;

namespace
{
    // The text character for each value of the Tiles layer, as in tools/rooms.py.
    char tileChar(int value)
    {
        switch (value) {
            case 1: return '#';
            case 2: return '%';
            case 3: return '-';
            case 4: return '^';
            case 5: return 'x';
            case 6: return '=';
            case 7: return 'o';
            default: return '.';
        }
    }

    const json& layer(const json& level, const std::string& identifier)
    {
        for (const json& instance : level.at("layerInstances")) {
            if (instance.at("__identifier") == identifier) return instance;
        }
        throw std::runtime_error("Room " + level.at("identifier").get<std::string>() + " has no " + identifier + " layer");
    }

    std::string field(const json& instances, const std::string& identifier, const std::string& fallback = "")
    {
        for (const json& f : instances) {
            if (f.at("__identifier") == identifier && f.at("__value").is_string()) return f.at("__value");
        }
        return fallback;
    }

    std::shared_ptr<Level> loadRoom(const json& data)
    {
        // The tiles, as rows of a text level, so they go through the same code as level files.
        const json& tiles = layer(data, "Tiles");
        const int width = tiles.at("__cWid"), height = tiles.at("__cHei");
        const json& csv = tiles.at("intGridCsv");
        std::vector<std::string> rows(static_cast<size_t>(height), std::string(static_cast<size_t>(width), '.'));
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) rows[y][x] = tileChar(csv.at(static_cast<size_t>(y * width + x)));
        }
        auto level = Level::fromRows(rows);

        for (const json& entity : layer(data, "Entities").at("entityInstances")) {
            const std::string kind = entity.at("__identifier");
            // LDtk counts rows from the top, the game from the bottom.
            const sf::Vector2i tile(entity.at("__grid")[0], height - 1 - entity.at("__grid")[1].get<int>());
            const json& fields = entity.at("fieldInstances");
            if (kind == "Start") level->setStart(tile);
            else if (kind == "Checkpoint") level->addCheckpoint(tile);
            else if (kind == "Goal") level->setGoal(tile);
            else if (kind == "Pickup") level->addPickup(field(fields, "ability"), tile);
            else if (kind == "Gem") level->addGem(tile);
            else if (kind == "Npc") level->addNpc(field(fields, "dialogue"), field(fields, "look"), tile);
            else if (kind == "Bench") level->addBench(tile);
            else if (kind == "Lever") level->addLever(tile);
        }
        if (!level->hasStart()) throw std::runtime_error("Room " + data.at("identifier").get<std::string>() + " has no Start");
        return level;
    }
}

std::vector<LevelInfo> loadWorld(const std::string& ldtkPath)
{
    std::ifstream file(ldtkPath);
    if (!file) throw std::runtime_error("Can't open the world: " + ldtkPath);
    json project;
    try {
        project = json::parse(file);
    } catch (const json::exception& e) {
        throw std::runtime_error("Can't read " + ldtkPath + ": " + e.what());
    }

    try {
        const json& levels = project.at("levels");
        if (levels.empty()) throw std::runtime_error("No rooms in " + ldtkPath);
        // LDtk's y points down; the game's map positions point up, from the lowest room's bottom.
        int bottom = 0;
        for (const json& level : levels) bottom = std::max(bottom, level.at("worldY").get<int>() + level.at("pxHei").get<int>());

        std::vector<LevelInfo> rooms;
        for (const json& level : levels) {
            LevelInfo info;
            info.name = level.at("identifier");
            info.title = field(level.at("fieldInstances"), "title", info.name);
            info.theme = field(level.at("fieldInstances"), "theme", Theme::DEFAULT);
            const int tile = static_cast<int>(Level::TILE_SIZE);
            info.position = {level.at("worldX").get<int>() / tile, (bottom - level.at("worldY").get<int>() - level.at("pxHei").get<int>()) / tile};
            info.level = loadRoom(level);
            rooms.push_back(std::move(info));
        }
        return rooms;
    } catch (const json::exception& e) {
        throw std::runtime_error("Unexpected layout in " + ldtkPath + ": " + e.what());
    }
}

LevelInfo loadSingleLevel(const std::string& textPath)
{
    LevelInfo info;
    info.name = std::filesystem::path(textPath).stem().string();
    info.title = "CUSTOM LEVEL";
    info.level = Level::loadFromFile(textPath);
    return info;
}
