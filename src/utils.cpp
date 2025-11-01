#include "utils.h"
#include <SFML/Graphics.hpp>
#include <cmath>

sf::Vector2f viewCoordinates(sf::Vector2f pos , sf::Vector2f centr)
{
    return {pos.x + 160.f - centr.x, -pos.y + 110.f + centr.y};
}

std::pair<int, int> tailNumbers(sf::Vector2f pos)
{
    constexpr float TILE_SIZE = 8.f;
    int x = static_cast<int>(std::floor(pos.x / TILE_SIZE));
    int y = static_cast<int>(std::floor(pos.y / TILE_SIZE));
    return {x, y};
}