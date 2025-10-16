#include "firebomb.h"

Firebomb::Firebomb(float startX, float startY)
    : speed(120.f)
{
    shape = sf::CircleShape(8.f); // Size
    shape.setOutlineThickness(1.f);
    shape.setOutlineColor(sf::Color::Red);
    shape.setFillColor(sf::Color::Transparent);
    shape.setPosition(startX, startY);
    velocity = sf::Vector2f(-speed, 0.f); // Speed and direction
}
