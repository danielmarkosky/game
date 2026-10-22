#pragma once

#include "src/Level.h"
#include "src/Theme.h"
#include <SFML/System/Vector2.hpp>
#include <memory>
#include <string>
#include <vector>

// A room of the world, ready to play.
struct LevelInfo
{
    std::string name;  // its identifier in the LDtk project, also its key in the save
    std::string title; // shown the first time you go in
    std::string theme = Theme::DEFAULT;
    sf::Vector2i position{}; // on the world map, in tiles: where the room's bottom-left tile is
    std::shared_ptr<Level> level;
};

// Every room in an LDtk project (assets/world.ldtk, made in https://ldtk.io or by
// tools/rooms.py), in the project's order. Uses the layers and entities described in
// assets/levels/README.md. Throws std::runtime_error if the file can't be read or isn't
// laid out like that, or a room has no Start.
std::vector<LevelInfo> loadWorld(const std::string& ldtkPath);

// One room from a text level file, for trying a level out on its own.
LevelInfo loadSingleLevel(const std::string& textPath);
