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

void drawDebugGrid(sf::RenderTarget& target, sf::Vector2f centr, int cols, int rows, float tileSize, sf::Color color)
{
    for (int x = 0; x <= cols; ++x) {
        float gx = x * tileSize;
        auto p1 = viewCoordinates(sf::Vector2f(gx, 0), centr);
        auto p2 = viewCoordinates(sf::Vector2f(gx, rows * tileSize), centr);
        sf::Vertex line[] = {
            sf::Vertex(sf::Vector2f(std::floor(p1.x), std::floor(p1.y)), color),
            sf::Vertex(sf::Vector2f(std::floor(p2.x), std::floor(p2.y)), color)
        };
        target.draw(line, 2, sf::Lines);
    }
    for (int y = 0; y <= rows; ++y) {
        float gy = y * tileSize;
        auto p1 = viewCoordinates(sf::Vector2f(0, gy), centr);
        auto p2 = viewCoordinates(sf::Vector2f(cols * tileSize, gy), centr);
        sf::Vertex line[] = {
            sf::Vertex(sf::Vector2f(std::floor(p1.x), std::floor(p1.y)), color),
            sf::Vertex(sf::Vector2f(std::floor(p2.x), std::floor(p2.y)), color)
        };
        target.draw(line, 2, sf::Lines);
    }

    sf::Vector2f logicPos(0.f, 0.f);
    auto screenPos = viewCoordinates(logicPos, centr);

    sf::CircleShape dot(2.f);
    dot.setFillColor(sf::Color::Red);
    dot.setOrigin(2.f, 2.f);
    dot.setPosition(std::floor(screenPos.x), std::floor(screenPos.y));
    target.draw(dot);
}

double distance(sf::Vector2f p1, sf::Vector2f p2)
{
    return std::sqrt(std::pow(p1.x - p2.x, 2) + std::pow(p1.y - p2.y, 2));
}

std::optional<sf::Vector2f> nearestSolidPoint(sf::Vector2f from, sf::Vector2f to, std::shared_ptr<const Level> level, float maxDistance)
{
    float length = static_cast<float>(distance(from, to));
    if (length == 0) return std::nullopt;
    sf::Vector2f dirNorm = (to - from) / length;
    float searchLimit = std::min(length, maxDistance);

    float currentDist = 0.0f;
    const float step = 1.0f;

    while (currentDist <= searchLimit) {
        sf::Vector2f currentPos = from + dirNorm * currentDist;

        auto [tileX, tileY] = tailNumbers(currentPos);
        if (level->isSolid(tileX, tileY)) {
            return sf::Vector2f(std::floor(currentPos.x), std::floor(currentPos.y));
        }

        currentDist += step;
    }

    return std::nullopt;
}

