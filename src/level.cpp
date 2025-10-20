#include "level.h"
#include <SFML/Graphics.hpp>
#include <string>

Level::Level() {}

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

bool Level::collidesWithWall(const sf::FloatRect& rect) const {
    for (const auto& wall : walls) {
        if (wall.getBounds().intersects(rect)) {
            return true;
        }
    }
    return false;
}
