#include "src/Config.h"
#include "src/Game.h"
#include "src/Physics.h"
#include <algorithm>
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        for (const std::string& problem : loadPhysics(config::assetPath("physics.txt"))) std::cerr << "physics.txt: " << problem << std::endl;
        // ./game                  the world, from the title screen
        // ./game room.txt         one text room on its own
        // ./game --room <name>    the world, starting in that room with every ability (for testing)
        const std::string arg = argc > 1 ? argv[1] : "";
        std::vector<LevelInfo> levels = argc > 1 && arg != "--room"
            ? std::vector<LevelInfo>{loadSingleLevel(arg)}
            : loadWorld(config::assetPath("world.ldtk"));
        size_t startRoom = 0;
        if (arg == "--room") {
            const std::string name = argc > 2 ? argv[2] : "";
            auto room = std::find_if(levels.begin(), levels.end(), [&](const LevelInfo& l) { return l.name == name; });
            if (room == levels.end()) {
                std::cerr << "No room called '" << name << "'. The rooms are:";
                for (const auto& l : levels) std::cerr << " " << l.name;
                std::cerr << std::endl;
                return 1;
            }
            startRoom = static_cast<size_t>(room - levels.begin());
        }
        Game game(levels, {.testing = argc > 1, .startRoom = startRoom});
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
