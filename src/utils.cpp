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

void drawDebugGrid(sf::RenderTarget& target, sf::Vector2f centr, int cols, int rows, float tileSize, sf::Color color) {


    for (int x = 0; x <= cols; ++x) {
        float gx = x * tileSize;
        auto p1 = viewCoordinates(sf::Vector2f(gx, 0), centr);
        auto p2 = viewCoordinates(sf::Vector2f(gx, rows * tileSize), centr);
        sf::Vertex line[] = {
            sf::Vertex(sf::Vector2f(std::floor(p1.x + 0.5f), std::floor(p1.y + 0.5f)), color),
            sf::Vertex(sf::Vector2f(std::floor(p2.x + 0.5f), std::floor(p2.y + 0.5f)), color)
        };
        target.draw(line, 2, sf::Lines);
    }
    for (int y = 0; y <= rows; ++y) {
        float gy = y * tileSize;
        auto p1 = viewCoordinates(sf::Vector2f(0, gy), centr);
        auto p2 = viewCoordinates(sf::Vector2f(cols * tileSize, gy), centr);
        sf::Vertex line[] = {
            sf::Vertex(sf::Vector2f(std::floor(p1.x + 0.5f), std::floor(p1.y + 0.5f)), color),
            sf::Vertex(sf::Vector2f(std::floor(p2.x + 0.5f), std::floor(p2.y + 0.5f)), color)
        };
        target.draw(line, 2, sf::Lines);
    }

    sf::Vector2f logicPos(0.f, 0.f);
    auto screenPos = viewCoordinates(logicPos, centr);

    sf::CircleShape dot(2.f);
    dot.setFillColor(sf::Color::Red);
    dot.setOrigin(2.f, 2.f);
    dot.setPosition(std::floor(screenPos.x + 0.5f), std::floor(screenPos.y + 0.5f));
    target.draw(dot);
}
