#include "duck.h"
#include <SFML/Graphics.hpp>

Duck::Duck(const sf::Vector2f& startPos, std::shared_ptr<Level> level)
    : Player(startPos, level)
{
    // Set duck-specific parameters
    shape.setSize(sf::Vector2f(16.f, 16.f)); // Example size for duck
    shape.setFillColor(sf::Color::Yellow);
    shape.setOutlineThickness(2.f);
    shape.setOutlineColor(sf::Color::Black);
    shape.setPosition(startPos);
    moveSpeed = 100.f;
    jumpStrength = 220.f;
    gravity = 400.f;
}
// ...existing code for update/draw if needed...

