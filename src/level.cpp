#include "level.h"
#include <SFML/Graphics.hpp>
#include <string>

Level::Level() {
    // Walls should be defined in derived classes, so base constructor does nothing
}

const std::vector<Wall>& Level::getWalls() const {
    return walls;
}

void Level::draw(sf::RenderWindow& window) const {
    for (const auto& wall : walls) {
        wall.draw(window);
    }
}

void Level::addWall(const Wall& wall) {
    walls.push_back(wall);
}
