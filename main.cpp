#include <SFML/Graphics.hpp>
#include "src/game.h"

// Our game's fixed internal resolution
constexpr unsigned int VIEW_WIDTH = 320;
constexpr unsigned int VIEW_HEIGHT = 180;

int main() {
    Game game;
    game.run();
    return 0;
}
