#pragma once

#include "src/player.h"
#include <memory>
#include <SFML/Graphics.hpp>

class Duck : public Player {
public:
    Duck(const sf::Vector2f& startPos, std::shared_ptr<Level> level);
    void draw(sf::RenderWindow& window) override;
};
