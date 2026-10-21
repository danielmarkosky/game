#include "src/Config.h"
#include "src/Game.h"
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        // Play one level file if given, otherwise all of them in order.
        std::vector<LevelInfo> levels = argc > 1
            ? std::vector<LevelInfo>{{argv[1], "CUSTOM LEVEL"}}
            : loadLevelList(config::assetPath("levels/levels.txt"));
        Game game(levels);
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
