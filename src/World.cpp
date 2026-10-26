#include "World.h"
#include "RoomFile.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
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
            case 8: return '~';
            case 9: return '&';
            case 10: return '_';
            case 11: return 'w';
            case 12: return 'm';
            case 13: return '?';
            case 14: return 't';
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

    entities::Fields fieldsOf(const json& instances)
    {
        entities::Fields fields;
        for (const json& f : instances) {
            if (f.at("__value").is_string()) fields[f.at("__identifier")] = f.at("__value");
        }
        return fields;
    }

    std::string get(const entities::Fields& fields, const std::string& name, const std::string& fallback = "")
    {
        const auto it = fields.find(name);
        return it == fields.end() || it->second.empty() ? fallback : it->second;
    }
}

namespace entities
{
    const std::vector<Kind>& all()
    {
        static const std::vector<Kind> kinds = {
            {'S', "Start"}, {'C', "Checkpoint"}, {'G', "Goal"}, {'P', "Pickup"}, {'*', "Gem"}, {'N', "Npc"}, {'B', "Bench"},
            {'L', "Lever"}, {'D', "Prop"}, {'Y', "Icicle"}, {'U', "Updraft"}, {'R', "Ring"}, {'Q', "Lamp"}, {'K', "Cart"},
            {'E', "CartStop"}, {'T', "Trial"},
        };
        return kinds;
    }

    std::string identifierOf(char character)
    {
        for (const Kind& kind : all()) {
            if (kind.character == character) return kind.identifier;
        }
        return "";
    }

    void add(Level& level, const std::string& kind, sf::Vector2i tile, const Fields& fields)
    {
        const auto f = [&](const std::string& name, const std::string& fallback = "") { return get(fields, name, fallback); };
        if (kind == "Start") level.setStart(tile);
        else if (kind == "Checkpoint") level.addCheckpoint(tile);
        else if (kind == "Goal") level.setGoal(tile);
        else if (kind == "Pickup") level.addPickup(f("ability", "rope"), tile);
        else if (kind == "Gem") level.addGem(tile);
        else if (kind == "Npc") level.addNpc(Npc{f("dialogue"), f("look"), {}, f("pose") == "sit", f("needs"), f("until")}, tile);
        else if (kind == "Bench") level.addBench(tile);
        else if (kind == "Lever") level.addLever(tile);
        else if (kind == "Prop") level.addProp(f("kind"), tile, f("needs"));
        else if (kind == "Icicle") level.addIcicle(tile);
        else if (kind == "Updraft") level.addUpdraft(tile);
        else if (kind == "Lamp") level.addLamp(tile);
        else if (kind == "Cart") level.addCart(tile, f("dir", "1") == "-1" ? -1 : 1, f("needs"));
        else if (kind == "CartStop") level.addCartStop(tile);
        else if (kind == "Trial") level.addTrial(f("route"), f("mark") == "finish", tile);
        else if (kind == "Ring") {
            // `to`: where its track ends, in tiles right and up from it, "6,0"; `period`: seconds there and back.
            int dx = 6, dy = 0;
            std::sscanf(f("to", "6,0").c_str(), "%d,%d", &dx, &dy);
            level.addRing(tile, {tile.x + dx, tile.y + dy}, std::strtof(f("period", "4").c_str(), nullptr));
        }
    }
}

void applyRoomSettings(Level& level, const entities::Fields& settings)
{
    level.setWind(std::strtof(get(settings, "wind", "0").c_str(), nullptr));
    level.setDark(get(settings, "dark") == "1");
    level.setSpace(get(settings, "space") == "1");
}

namespace
{
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
            // LDtk counts rows from the top, the game from the bottom.
            const sf::Vector2i tile(entity.at("__grid")[0], height - 1 - entity.at("__grid")[1].get<int>());
            entities::add(*level, entity.at("__identifier"), tile, fieldsOf(entity.at("fieldInstances")));
        }
        applyRoomSettings(*level, fieldsOf(data.at("fieldInstances")));
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
            info.region = field(level.at("fieldInstances"), "region");
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
    // A room made in the room editor, with its title, theme and entities' fields: as it is.
    std::ifstream file(textPath);
    std::stringstream text;
    text << file.rdbuf();
    if (RoomFile::looksLikeOne(text.str())) return RoomFile::load(textPath).toLevel();
    LevelInfo info;
    info.name = std::filesystem::path(textPath).stem().string();
    info.title = info.name; // "playground"
    info.level = Level::loadFromFile(textPath);
    return info;
}
