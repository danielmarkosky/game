#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include "wall.h"

class Level {
public:
    Level();
    virtual ~Level() = default;

    // Returns all walls in the level
    const std::vector<Wall>& getWalls() const;

    // Draws all walls
    virtual void draw(sf::RenderWindow& window) const;

protected:
    void addWall(const Wall& wall);

    std::vector<Wall> walls;
};