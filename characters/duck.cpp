#include "duck.h"
#include <SFML/Graphics.hpp>

Duck::Duck(const sf::Vector2f& startPos, std::shared_ptr<Level> level)
    : Player(startPos, level) // Call base Player constructor
{
    // Set duck-specific parameters
    shape.setSize(sf::Vector2f(10.f, 16.f)); // Example size for duck
    shape.setFillColor(sf::Color::Transparent); // Make duck not filled
    shape.setOutlineThickness(1.f); // Make outline thicker for visibility
    shape.setOutlineColor(sf::Color::Yellow); // Ensure outline is black
    shape.setPosition(startPos);
    moveSpeed = 100.f;
    jumpStrength = 220.f;
    gravity = 400.f;
}
// Duck uses Player's update/draw methods by default
// ...existing code for update/draw if needed...
