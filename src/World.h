#pragma once

#include "src/Level.h"
#include "src/Theme.h"
#include <SFML/System/Vector2.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

// A room of the world, ready to play.
struct LevelInfo
{
    std::string name;  // its identifier in the LDtk project, also its key in the save
    std::string title; // shown the first time you go in
    std::string theme = Theme::DEFAULT;
    std::string region; // one of assets/regions.txt, for the map
    sf::Vector2i position{}; // on the world map, in tiles: where the room's bottom-left tile is
    std::shared_ptr<Level> level;
};

// The things in rooms besides the tiles, as tools/rooms.py has them: each by its identifier in the
// LDtk project and its character in text rooms, with its fields by name (see
// assets/levels/README.md). The same for the LDtk project and the room editor's files.
namespace entities
{
    using Fields = std::map<std::string, std::string>;
    struct Kind
    {
        char character;          // 'S'
        const char* identifier;  // "Start"
    };
    const std::vector<Kind>& all();
    // The identifier of the one written with that character, or "" if it's a tile's.
    std::string identifierOf(char character);
    // Puts one in the level, at its tile. Fields left out are the defaults.
    void add(Level& level, const std::string& identifier, sf::Vector2i tile, const Fields& fields);
}

// A room's own settings besides its title, theme and region: "wind" (px/s each second, + to the
// right), "dark" ("1") and "space" ("1": no gravity).
void applyRoomSettings(Level& level, const entities::Fields& settings);

// Every room in an LDtk project (assets/world.ldtk, made in https://ldtk.io or by
// tools/rooms.py), in the project's order. Uses the layers and entities described in
// assets/levels/README.md. Throws std::runtime_error if the file can't be read or isn't
// laid out like that, or a room has no Start.
std::vector<LevelInfo> loadWorld(const std::string& ldtkPath);

// One room from a text level file, for trying a level out on its own: a plain one (just the
// tiles), or one made in the room editor (see src/RoomFile.h).
LevelInfo loadSingleLevel(const std::string& textPath);
